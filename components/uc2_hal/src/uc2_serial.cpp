// T1.3 (no Arduino): uc2::Serial_ implementation — direct ESP-IDF UART
// driver calls (IDF 6.1 classic uart_* API). See uc2_serial.h for the
// surface and rationale.
#include "uc2_hal/uc2_serial.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace uc2 {

void Serial_::begin(uint32_t baud, int config, int rxPin, int txPin)
{
    if (installed_)
    {
        return;
    }

    // 0 RX/TX queue entries + buffer sizes: buffered driver, polled reads
    // (uart_read_bytes with ticks) — no FreeRTOS queue needed.
    esp_err_t err = uart_driver_install(port_, static_cast<int>(rx_buf_),
                                        static_cast<int>(tx_buf_), 0, nullptr, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        return;  // port busy/invalid — leave uninstalled
    }

    // IDF 6.1: no UART_CONFIG_DEFAULT() — zero-init the (larger) struct and
    // set the fields (rx_glitch_filt_thresh 0 = RX filtering off).
    uart_config_t cfg = {};
    cfg.baud_rate = static_cast<int>(baud);
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.rx_flow_ctrl_thresh = 0;
    cfg.source_clk = UART_SCLK_DEFAULT;
    if (config == SERIAL_8E1)
    {
        cfg.parity = UART_PARITY_EVEN;
    }
    else if (config == SERIAL_8O1)
    {
        cfg.parity = UART_PARITY_ODD;
    }
    uart_param_config(port_, &cfg);

    // Explicit pins only when given; -1 = leave the port default untouched.
    if (txPin >= 0 || rxPin >= 0)
    {
        uart_set_pin(port_, txPin >= 0 ? txPin : UART_PIN_NO_CHANGE,
                     rxPin >= 0 ? rxPin : UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }

    // Arduino setTxTimeoutMs: inter-byte gap that counts as "tx done".
    if (tx_timeout_ms_ != 0)
    {
        const uint16_t idleBits =
            static_cast<uint16_t>(baud * tx_timeout_ms_ / 1000);
        uart_set_tx_idle_num(port_, idleBits);
    }

    installed_ = true;
}

void Serial_::end()
{
    if (!installed_)
    {
        return;
    }
    uart_driver_delete(port_);
    installed_ = false;
}

// ── RX ─────────────────────────────────────────────────────────────────────

int Serial_::read()
{
    uint8_t b = 0;
    // ticks 0 = non-blocking (xQueueReceive 0 / FIFO poll, no wait).
    return uart_read_bytes(port_, &b, 1, 0) == 1 ? static_cast<int>(b) : -1;
}

int Serial_::available()
{
    size_t len = 0;
    uart_get_buffered_data_len(port_, &len);
    return static_cast<int>(len);
}

size_t Serial_::readBytes(char *buffer, size_t length, uint32_t timeoutMs)
{
    const uint32_t timeout = timeoutMs == UINT32_MAX ? timeout_ms_ : timeoutMs;
    if (timeoutMs == UINT32_MAX && length > 0)
    {
        // Arduino readBytes semantics: wait for the first byte up to the
        // timeout, then drain whatever arrives (no further waiting).
        uint8_t first;
        if (uart_read_bytes(port_, &first, 1, pdMS_TO_TICKS(timeout)) != 1)
        {
            return 0;
        }
        buffer[0] = static_cast<char>(first);
        if (length == 1)
        {
            return 1;
        }
        const int rest = uart_read_bytes(port_, buffer + 1, length - 1, 0);
        return static_cast<size_t>(rest > 0 ? rest : 0) + 1;
    }
    if (timeoutMs == 0 && length > 0)
    {
        const int n = uart_read_bytes(port_, buffer, length, 0);
        return n > 0 ? static_cast<size_t>(n) : 0;
    }
    const int n = uart_read_bytes(port_, buffer, length, pdMS_TO_TICKS(timeout));
    return n > 0 ? static_cast<size_t>(n) : 0;
}

// ── TX ─────────────────────────────────────────────────────────────────────

void Serial_::flush()
{
    // uart_flush: wait for the driver tx buffer + FIFO to empty.
    uart_flush(port_);
    uart_wait_tx_done(port_, portMAX_DELAY);
}

size_t Serial_::write(uint8_t b)
{
    const int n = uart_write_bytes(port_, &b, 1);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

size_t Serial_::write(const uint8_t *data, size_t length)
{
    if (length == 0)
    {
        return 0;
    }
    const int n = uart_write_bytes(port_, data, length);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

size_t Serial_::write(const char *s)
{
    return s ? write(reinterpret_cast<const uint8_t *>(s), std::strlen(s)) : 0;
}

int Serial_::printf(const char *format, ...)
{
    char buf[512];
    va_list args;
    va_start(args, format);
    const int n = std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    if (n > 0)
    {
        write(reinterpret_cast<const uint8_t *>(buf),
              static_cast<size_t>(n < static_cast<int>(sizeof(buf))
                                      ? n
                                      : static_cast<int>(sizeof(buf)) - 1));
    }
    return n;
}

// ── print / println ────────────────────────────────────────────────────────

void Serial_::writeStr(const char *s)
{
    if (s)
    {
        write(reinterpret_cast<const uint8_t *>(s), std::strlen(s));
    }
}

void Serial_::print(const char *s) { writeStr(s); }
void Serial_::print(char c)
{
    uint8_t b = static_cast<uint8_t>(c);
    write(b);
}
void Serial_::print(uint8_t v)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(v));
    writeStr(buf);
}
void Serial_::print(int v)
{
    char buf[12];
    std::snprintf(buf, sizeof(buf), "%d", v);
    writeStr(buf);
}
void Serial_::print(unsigned int v)
{
    char buf[12];
    std::snprintf(buf, sizeof(buf), "%u", v);
    writeStr(buf);
}
void Serial_::print(long v)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%ld", v);
    writeStr(buf);
}
void Serial_::print(unsigned long v)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%lu", v);
    writeStr(buf);
}
void Serial_::print(float v) { print(static_cast<double>(v)); }
void Serial_::print(double v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    writeStr(buf);
}
void Serial_::print(bool v) { writeStr(v ? "1" : "0"); }

void Serial_::println() { writeStr("\n"); }
void Serial_::println(const char *s)
{
    writeStr(s);
    writeStr("\n");
}
void Serial_::println(char c)
{
    print(c);
    writeStr("\n");
}
void Serial_::println(uint8_t v)
{
    print(v);
    writeStr("\n");
}
void Serial_::println(int v)
{
    print(v);
    writeStr("\n");
}
void Serial_::println(unsigned int v)
{
    print(v);
    writeStr("\n");
}
void Serial_::println(long v)
{
    print(v);
    writeStr("\n");
}
void Serial_::println(unsigned long v)
{
    print(v);
    writeStr("\n");
}
void Serial_::println(float v)
{
    print(v);
    writeStr("\n");
}
void Serial_::println(double v)
{
    print(v);
    writeStr("\n");
}
void Serial_::println(bool v)
{
    print(v);
    writeStr("\n");
}

}  // namespace uc2

// Arduino-style globals (global scope, like `ESP` in uc2_esp.h).
uc2::Serial_ Serial(UART_NUM_0);
uc2::Serial_ Serial1(UART_NUM_2);
