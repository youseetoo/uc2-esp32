#include "I2cBridge.h"
#include "PinConfig.h"
#ifdef I2C_BRIDGE_CONTROLLER

#include <Arduino.h>
#include <Wire.h>

#ifdef CAN_CONTROLLER_CANOPEN
extern "C" {
#include "OD.h"
}
#endif

namespace I2cBridge
{
    // Command-blob byte offsets (see I2cBridge.h).
    static constexpr uint8_t OFF_ADDR   = 0;
    static constexpr uint8_t OFF_FLAGS  = 1;
    static constexpr uint8_t OFF_RDLEN  = 2;
    static constexpr uint8_t OFF_WRLEN  = 3;
    static constexpr uint8_t OFF_DELAY  = 4;
    static constexpr uint8_t OFF_WRDATA = 5;

    static constexpr uint8_t FLAG_STOP_AFTER_WRITE = 0x01;

    // Status codes written to OD 0x2352 (distinct so failures are diagnosable).
    static constexpr uint8_t ST_IDLE        = 0x00; // ready / last op was idle
    static constexpr uint8_t ST_BUSY        = 0x01; // transaction in flight
    static constexpr uint8_t ST_DONE_OK     = 0x02; // completed, data valid
    // 0x81..0x85 = Wire.endTransmission() code (1 too long, 2 addr NACK,
    //              3 data NACK, 4 other, 5 timeout) OR'd with 0x80
    static constexpr uint8_t ST_ERR_WRITE   = 0x80;
    static constexpr uint8_t ST_ERR_SHORT   = 0x90; // fewer bytes than requested
    static constexpr uint8_t ST_ERR_PARAM   = 0xA0; // rd_len/wr_len out of range

    static bool s_ready = false;

    // Non-blocking state machine.
    enum Phase { PHASE_IDLE, PHASE_WAIT_DELAY };
    static Phase    s_phase   = PHASE_IDLE;
    static uint8_t  s_addr    = 0;
    static uint8_t  s_rdLen   = 0;
    static uint32_t s_delayT0 = 0;
    static uint32_t s_delayMs = 0;

    static uint8_t cmdCapacity() { return (uint8_t)sizeof(OD_RAM.x2350_i2c_command); }
    static uint8_t respCapacity() { return (uint8_t)sizeof(OD_RAM.x2353_i2c_response); }

    // Issue the read phase, copy bytes into the response OD entry, finish.
    static void finishRead()
    {
        uint8_t got = (uint8_t)Wire.requestFrom((int)s_addr, (int)s_rdLen); // returns the number of bytes actually read
        uint8_t n = 0;
        while (Wire.available() && n < s_rdLen && n < respCapacity())
            OD_RAM.x2353_i2c_response[n++] = (uint8_t)Wire.read();
        OD_RAM.x2354_i2c_resp_len = n;
        OD_RAM.x2352_i2c_status   = (n == s_rdLen) ? ST_DONE_OK : ST_ERR_SHORT;
        (void)got; // suppress unused warning (Wire.requestFrom() returns the same value)
    }

    // Mark the transaction complete: publish status and release the trigger so
    // the polling master sees it finish. Called from every terminal path.
    static void complete(uint8_t status)
    {
        OD_RAM.x2352_i2c_status  = status;
        OD_RAM.x2351_i2c_trigger = 0;   // release AFTER status is set
        s_phase = PHASE_IDLE;
    }

    void setup()
    {
        if (pinConfig.I2C_SDA < 0 || pinConfig.I2C_SCL < 0) {
            log_i("I2cBridge disabled (I2C_SDA/SCL not set)");
            return;
        }
        Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);
        s_ready = true;
#ifdef CAN_CONTROLLER_CANOPEN
        OD_RAM.x2351_i2c_trigger  = 0;
        OD_RAM.x2352_i2c_status   = ST_IDLE;
        OD_RAM.x2354_i2c_resp_len = 0;
#endif
        log_i("I2cBridge on SDA=GPIO%d SCL=GPIO%d @100kHz",
              pinConfig.I2C_SDA, pinConfig.I2C_SCL);
    }

    void loop()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        if (!s_ready) return;

        // Waiting out a device conversion delay between write and read.
        if (s_phase == PHASE_WAIT_DELAY) {
            if ((millis() - s_delayT0) < s_delayMs) return;
            finishRead();
            OD_RAM.x2351_i2c_trigger = 0;   // read done → release trigger
            s_phase = PHASE_IDLE;
            return;
        }

        // Idle: nothing to do unless the master armed a transaction.
        if (OD_RAM.x2351_i2c_trigger == 0) return;

        OD_RAM.x2352_i2c_status = ST_BUSY;

        const uint8_t* cmd   = OD_RAM.x2350_i2c_command;
        s_addr               = cmd[OFF_ADDR] & 0x7F;
        const uint8_t flags  = cmd[OFF_FLAGS];
        s_rdLen              = cmd[OFF_RDLEN];
        const uint8_t wrLen  = cmd[OFF_WRLEN];
        s_delayMs            = cmd[OFF_DELAY];

        // Bounds: write payload must fit in the command buffer, read length in
        // the response buffer.
        if (s_rdLen > respCapacity() || wrLen > (uint8_t)(cmdCapacity() - OFF_WRDATA)) {
            complete(ST_ERR_PARAM);
            return;
        }

        // --- Address probe (bus scan): no write, no read -------------------
        if (wrLen == 0 && s_rdLen == 0) {
            Wire.beginTransmission(s_addr);
            uint8_t err = Wire.endTransmission(true);
            complete(err ? (uint8_t)(ST_ERR_WRITE | (err & 0x0F)) : ST_DONE_OK);
            return;
        }

        // --- Write phase ---------------------------------------------------
        if (wrLen > 0) {
            // Release the bus (STOP) when the device asks for it, when a delay
            // follows, or when there is no read — otherwise repeated-START.
            bool sendStop = (flags & FLAG_STOP_AFTER_WRITE) ||
                            (s_delayMs > 0) || (s_rdLen == 0);
            Wire.beginTransmission(s_addr);
            Wire.write(cmd + OFF_WRDATA, wrLen);
            uint8_t err = Wire.endTransmission(sendStop);
            if (err != 0) {
                complete((uint8_t)(ST_ERR_WRITE | (err & 0x0F)));
                return;
            }
        }

        // --- Read phase (immediate, deferred, or none) ---------------------
        if (s_rdLen == 0) {
            complete(ST_DONE_OK);
            return;
        }
        if (s_delayMs > 0) {
            s_delayT0 = millis();
            s_phase   = PHASE_WAIT_DELAY;   // trigger stays armed until read done
            return;
        }
        finishRead();
        OD_RAM.x2351_i2c_trigger = 0;
        s_phase = PHASE_IDLE;
#endif // CAN_CONTROLLER_CANOPEN
    }

    uint8_t execute(uint8_t addr, uint8_t flags, uint8_t rdLen,
                    const uint8_t* wr, uint8_t wrLen, uint8_t delayMs,
                    uint8_t* resp, uint8_t respCap, uint8_t* respLen)
    {
        if (respLen) *respLen = 0;
        if (!s_ready) return ST_ERR_PARAM;
        addr &= 0x7F;
        if (rdLen > respCap) return ST_ERR_PARAM;

        // Address probe (bus scan): no write, no read → just check for an ACK.
        if (wrLen == 0 && rdLen == 0) {
            Wire.beginTransmission(addr);
            uint8_t err = Wire.endTransmission(true);
            return err ? (uint8_t)(ST_ERR_WRITE | (err & 0x0F)) : ST_DONE_OK;
        }

        // Write phase.
        if (wrLen > 0) {
            bool sendStop = (flags & FLAG_STOP_AFTER_WRITE) ||
                            (delayMs > 0) || (rdLen == 0);
            Wire.beginTransmission(addr);
            if (wr && wrLen) Wire.write(wr, wrLen);
            uint8_t err = Wire.endTransmission(sendStop);
            if (err != 0) return (uint8_t)(ST_ERR_WRITE | (err & 0x0F));
        }

        if (rdLen == 0) return ST_DONE_OK;
        if (delayMs > 0) delay(delayMs);

        // Read phase.
        uint8_t got = (uint8_t)Wire.requestFrom((int)addr, (int)rdLen);
        uint8_t n = 0;
        while (Wire.available() && n < rdLen && n < respCap)
            resp[n++] = (uint8_t)Wire.read();
        if (respLen) *respLen = n;
        (void)got;
        return (n == rdLen) ? ST_DONE_OK : ST_ERR_SHORT;
    }
}

#endif // I2C_BRIDGE_CONTROLLER
