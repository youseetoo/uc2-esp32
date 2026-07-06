// I2cBridge.h — generic raw-I2C passthrough for the CANopen GPIO slave.
//
// The master pushes a self-contained I2C transaction into the slave's OD and
// reads the result back. No per-device driver lives on the ESP32: the register
// map for any sensor (SHT45, TSL2591, …) is assembled on the Python (UC2-REST)
// side and shipped as an opaque byte blob. This makes "attach any I2C device
// and read it over CAN" possible without reflashing.
//
// ── OD interface (all SDO-only, GPIO slave, indices 0x2350-0x2354) ──────────
//   0x2350 i2c_command  octet[40] rw  request blob (see layout below)
//   0x2351 i2c_trigger  u8        rw  master writes !=0 → execute the command
//   0x2352 i2c_status   u8        ro  0 idle/ready, 1 busy, 2 done-ok,
//                                     0x80|err = failure (err = Wire code 1..5)
//   0x2353 i2c_response octet[40] ro  bytes read back
//   0x2354 i2c_resp_len u8        ro  number of valid bytes in 0x2353
//
// ── Command blob layout (byte offsets into 0x2350) ─────────────────────────
//   [0] addr7     7-bit I2C address
//   [1] flags     bit0 = 1 → send STOP after the write phase (device wants a
//                 full stop before the read, e.g. SHT4x); 0 → repeated-START
//   [2] rd_len    number of bytes to read back (0..RESP_MAX)
//   [3] wr_len    number of bytes to write     (0..CMD_MAX-5)
//   [4] delay_ms  delay between write and read phases (0..255)
//   [5..] wr_bytes the write payload (register pointer, command, config, …)
//
// The read/delay/write phases are serviced by a small non-blocking state
// machine driven from loop(), so a device's conversion delay never stalls the
// slave's CAN/collision loop.
//
// Compiled only when I2C_BRIDGE_CONTROLLER is defined; wired into the GPIO
// slave via GpioCanSlave (guarded by pinConfig.I2C_SDA >= 0).

#pragma once
#include <stdint.h>

namespace I2cBridge
{
    // Wire.begin on pinConfig.I2C_SDA / I2C_SCL. No-op if either pin is
    // disabled (< 0). Safe to call once from GpioCanSlave::setup().
    void setup();

    // Services the transaction state machine. Cheap when idle. Call every
    // loop iteration (not throttled) so conversion delays resolve promptly.
    void loop();

    // Synchronous one-shot transaction — used by the LOCAL /i2c_act path on
    // the GPIO slave (a command handler runs in the same task as loop(), so it
    // cannot wait for the async state machine). Blocks for delayMs. Writes up
    // to respCap bytes into resp, sets *respLen, and returns the status byte
    // (2 = ok, 0x8x/0x90/0xA0 = error — see the OD 0x2352 codes in the .cpp).
    uint8_t execute(uint8_t addr, uint8_t flags, uint8_t rdLen,
                    const uint8_t* wr, uint8_t wrLen, uint8_t delayMs,
                    uint8_t* resp, uint8_t respCap, uint8_t* respLen);
}
