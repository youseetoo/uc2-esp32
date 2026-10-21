// I2cBridge.c — generic raw-I2C passthrough for the CANopen GPIO slave.
// T4.1: pure C (was I2cBridge.cpp, namespace I2cBridge).
#include "I2cBridge.h"
#include "PinConfigDefault.h"
#ifdef I2C_BRIDGE_CONTROLLER

#include <uc2_hal/uc2_log.h>
#include <uc2_hal/uc2_misc.h>
#include <uc2_hal/uc2_time.h>
#include <uc2_hal/uc2_wire_c.h>

#ifdef CAN_CONTROLLER_CANOPEN
#include "OD.h"
#endif

static const char* TAG = "I2cBridge";

// Command-blob byte offsets (see I2cBridge.h).
static const uint8_t OFF_ADDR   = 0;
static const uint8_t OFF_FLAGS  = 1;
static const uint8_t OFF_RDLEN  = 2;
static const uint8_t OFF_WRLEN  = 3;
static const uint8_t OFF_DELAY  = 4;
static const uint8_t OFF_WRDATA = 5;

static const uint8_t FLAG_STOP_AFTER_WRITE = 0x01;

// Status codes written to OD 0x2352 (distinct so failures are diagnosable).
static const uint8_t ST_IDLE        = 0x00; // ready / last op was idle
static const uint8_t ST_BUSY        = 0x01; // transaction in flight
static const uint8_t ST_DONE_OK     = 0x02; // completed, data valid
// 0x81..0x85 = Wire.endTransmission() code (1 too long, 2 addr NACK,
//              3 data NACK, 4 other, 5 timeout) OR'd with 0x80
static const uint8_t ST_ERR_WRITE   = 0x80;
static const uint8_t ST_ERR_SHORT   = 0x90; // fewer bytes than requested
static const uint8_t ST_ERR_PARAM   = 0xA0; // rd_len/wr_len out of range

static bool s_ready = false;

// Non-blocking state machine.
typedef enum { PHASE_IDLE, PHASE_WAIT_DELAY } Phase;
static Phase    s_phase   = PHASE_IDLE;
static uint8_t  s_addr    = 0;
static uint8_t  s_rdLen   = 0;
static uint32_t s_delayT0 = 0;
static uint32_t s_delayMs = 0;

#ifdef CAN_CONTROLLER_CANOPEN
static uint8_t cmdCapacity(void) { return (uint8_t)sizeof(OD_RAM.x2350_i2c_command); }
static uint8_t respCapacity(void) { return (uint8_t)sizeof(OD_RAM.x2353_i2c_response); }

// Issue the read phase, copy bytes into the response OD entry, finish.
static void finishRead(void)
{
    (void)wire_request_from(s_addr, (int)s_rdLen); // returns the number of bytes actually read
    uint8_t n = 0;
    while (wire_available() > 0 && n < s_rdLen && n < respCapacity())
        OD_RAM.x2353_i2c_response[n++] = (uint8_t)wire_read();
    OD_RAM.x2354_i2c_resp_len = n;
    OD_RAM.x2352_i2c_status   = (n == s_rdLen) ? ST_DONE_OK : ST_ERR_SHORT;
}

// Mark the transaction complete: publish status and release the trigger so
// the polling master sees it finish. Called from every terminal path.
static void complete(uint8_t status)
{
    OD_RAM.x2352_i2c_status  = status;
    OD_RAM.x2351_i2c_trigger = 0;   // release AFTER status is set
    s_phase = PHASE_IDLE;
}
#endif // CAN_CONTROLLER_CANOPEN

void i2c_bridge_setup(void)
{
    if (pinConfig.I2C_SDA < 0 || pinConfig.I2C_SCL < 0) {
        log_i("I2cBridge disabled (I2C_SDA/SCL not set)");
        return;
    }
    wire_begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);
    s_ready = true;
#ifdef CAN_CONTROLLER_CANOPEN
    OD_RAM.x2351_i2c_trigger  = 0;
    OD_RAM.x2352_i2c_status   = ST_IDLE;
    OD_RAM.x2354_i2c_resp_len = 0;
#endif
    log_i("I2cBridge on SDA=GPIO%d SCL=GPIO%d @100kHz",
          pinConfig.I2C_SDA, pinConfig.I2C_SCL);
}

void i2c_bridge_loop(void)
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
        wire_begin_transmission(s_addr);
        uint8_t err = wire_end_transmission(true);
        complete(err ? (uint8_t)(ST_ERR_WRITE | (err & 0x0F)) : ST_DONE_OK);
        return;
    }

    // --- Immediate write+read: one transaction (repeated START) --------
    if (wrLen > 0 && s_rdLen > 0 && s_delayMs == 0 &&
        !(flags & FLAG_STOP_AFTER_WRITE)) {
        uint8_t ec = 0;
        int n = wire_write_read(s_addr, cmd + OFF_WRDATA, (int)wrLen,
                                OD_RAM.x2353_i2c_response, (int)s_rdLen, &ec);
        OD_RAM.x2354_i2c_resp_len = (n > 0) ? (uint8_t)n : 0;
        OD_RAM.x2352_i2c_status   = (n < 0) ? (uint8_t)(ST_ERR_WRITE | (ec & 0x0F))
                                            : (n == s_rdLen) ? ST_DONE_OK : ST_ERR_SHORT;
        OD_RAM.x2351_i2c_trigger  = 0;
        s_phase = PHASE_IDLE;
        return;
    }

    // --- Write phase (always with STOP; repeated START handled above) --
    if (wrLen > 0) {
        wire_begin_transmission(s_addr);
        wire_write_buf(cmd + OFF_WRDATA, wrLen);
        uint8_t err = wire_end_transmission(true);
        if (err != 0) {
            complete((uint8_t)(ST_ERR_WRITE | (err & 0x0F)));
            return;
        }
    }

    // --- Read phase (deferred, or none) --------------------------------
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

uint8_t i2c_bridge_execute(uint8_t addr, uint8_t flags, uint8_t rd_len,
                           const uint8_t* wr, uint8_t wr_len, uint8_t delay_ms,
                           uint8_t* resp, uint8_t resp_cap, uint8_t* resp_len)
{
    if (resp_len) *resp_len = 0;
    if (!s_ready) return ST_ERR_PARAM;
    addr &= 0x7F;
    if (rd_len > resp_cap) return ST_ERR_PARAM;

    // Address probe (bus scan): no write, no read → just check for an ACK.
    if (wr_len == 0 && rd_len == 0) {
        wire_begin_transmission(addr);
        uint8_t err = wire_end_transmission(true);
        return err ? (uint8_t)(ST_ERR_WRITE | (err & 0x0F)) : ST_DONE_OK;
    }

    // Immediate write+read: one transaction (repeated START).
    if (wr_len > 0 && rd_len > 0 && delay_ms == 0 && !(flags & FLAG_STOP_AFTER_WRITE)) {
        uint8_t ec = 0;
        int n = wire_write_read(addr, wr, (int)wr_len, resp, (int)rd_len, &ec);
        if (resp_len) *resp_len = (n > 0) ? (uint8_t)n : 0;
        if (n < 0) return (uint8_t)(ST_ERR_WRITE | (ec & 0x0F));
        return (n == rd_len) ? ST_DONE_OK : ST_ERR_SHORT;
    }

    // Write phase (always with STOP).
    if (wr_len > 0) {
        wire_begin_transmission(addr);
        if (wr && wr_len) wire_write_buf(wr, wr_len);
        uint8_t err = wire_end_transmission(true);
        if (err != 0) return (uint8_t)(ST_ERR_WRITE | (err & 0x0F));
    }

    if (rd_len == 0) return ST_DONE_OK;
    if (delay_ms > 0) delay(delay_ms);

    // Read phase.
    uint8_t n = 0;
    (void)wire_request_from(addr, (int)rd_len);
    while (n < rd_len && n < resp_cap) {
        int b = wire_read();
        if (b < 0) break;
        resp[n++] = (uint8_t)b;
    }
    if (resp_len) *resp_len = n;
    return (n == rd_len) ? ST_DONE_OK : ST_ERR_SHORT;
}

#endif // I2C_BRIDGE_CONTROLLER
