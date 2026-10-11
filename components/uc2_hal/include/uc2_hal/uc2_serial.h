// T1.3 (no Arduino): uc2::Serial_ — C++ UART façade over the ESP-IDF UART
// driver (esp_driver_uart, classic API — fully present in IDF 6.1:
// uart_driver_install/param_config/set_pin/write_bytes/read_bytes/…).
//
// API surface = exactly what main/ uses (grep-verified 2026-10-11,
// 131 sites):
//   begin(baud[, config[, rxPin, txPin]]) / end
//   print / println — single-arg only (main/ has zero 2-arg calls):
//     const char*, char, uint8_t, int, unsigned, long, unsigned long,
//     float, double, bool
//   printf(fmt, ...)
//   write(uint8_t) / write(const uint8_t*, size_t) / write(const char*)
//   read() -> int (-1 when nothing) / readBytes(buf, len) -> size_t
//   available() / flush() / setTimeout(ms)
//   setRxBufferSize / setTxBufferSize (applied at the NEXT begin — main/
//     sets both before begin, like the Arduino core)
//   setTxTimeoutMs(ms) (0 = off) / setDebugOutput(bool) (no-op: the IDF
//     console owns the console port)
//
// Globals (global scope, Arduino-style — like `ESP` in uc2_esp.h):
//   Serial  = UART0 — the device serial protocol
//   Serial1 = UART2 — TMC stepper UART (Serial1.begin(115200, SERIAL_8N1, rx, tx))
//
// Console: on ESP32-S3 the IDF console is USB-Serial-JTAG by default, so
// UART0 is free for the app (no fight over the port). A classic-ESP32
// build with a UART console must redirect CONFIG_ESP_CONSOLE first — not a
// UC2 target chip.
#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/uart.h"

// Arduino-style port-format constants (values as in the Arduino core).
#define SERIAL_8N1 0x00
#define SERIAL_8E1 0x04
#define SERIAL_8O1 0x06

namespace uc2 {

class Serial_
{
public:
    explicit Serial_(uart_port_t port) : port_(port) {}
    ~Serial_() { end(); }
    Serial_(const Serial_ &) = delete;
    Serial_ &operator=(const Serial_ &) = delete;

    // rxPin/txPin: -1 = keep the port's default pins (uart_set_pin untouched).
    void begin(uint32_t baud, int config = SERIAL_8N1, int rxPin = -1, int txPin = -1);
    void end();

    // ── RX ─────────────────────────────────────────────────────────────────
    int read();  // next byte, or -1 when nothing is buffered (non-blocking)
    int available();
    // Returns the number of bytes actually read (<= length). Defaults to the
    // setTimeout() value, like Arduino's readBytes.
    size_t readBytes(char *buffer, size_t length, uint32_t timeoutMs = UINT32_MAX);

    // ── TX ─────────────────────────────────────────────────────────────────
    void flush();  // wait until everything queued is out (Arduino semantics)
    size_t write(uint8_t b);
    size_t write(const uint8_t *data, size_t length);
    size_t write(const char *s);
    int printf(const char *format, ...);

    void print(const char *s);
    void print(char c);
    void print(uint8_t v);
    void print(int v);
    void print(unsigned int v);
    void print(long v);
    void print(unsigned long v);
    void print(float v);
    void print(double v);
    void print(bool v);

    void println();
    void println(const char *s);
    void println(char c);
    void println(uint8_t v);
    void println(int v);
    void println(unsigned int v);
    void println(long v);
    void println(unsigned long v);
    void println(float v);
    void println(double v);
    void println(bool v);

    // ── options ────────────────────────────────────────────────────────────
    void setTimeout(uint32_t ms) { timeout_ms_ = ms; }
    // Applied at the next begin() (the driver buffers are fixed at install
    // time — main/ sets these before begin, like the Arduino core).
    void setRxBufferSize(size_t bytes) { rx_buf_ = bytes; }
    void setTxBufferSize(size_t bytes) { tx_buf_ = bytes; }
    void setTxTimeoutMs(uint32_t ms) { tx_timeout_ms_ = ms; }
    void setDebugOutput(bool) {}  // console driver owns the console port

private:
    uart_port_t port_;
    bool installed_ = false;
    uint32_t timeout_ms_ = 1;      // Arduino default
    uint32_t tx_timeout_ms_ = 0;   // Arduino default: no inter-byte timeout
    size_t rx_buf_ = 2048;
    size_t tx_buf_ = 2048;

    void writeStr(const char *s);
};

}  // namespace uc2

// Arduino-style globals (global scope, like `extern const EspClass ESP;`).
extern uc2::Serial_ Serial;
extern uc2::Serial_ Serial1;
