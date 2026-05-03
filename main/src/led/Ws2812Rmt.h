#pragma once
#include <stdint.h>
#include <stddef.h>

// Minimal WS2812 (NeoPixel) driver that talks to the RMT peripheral with the
// IDF driver installed using ESP_INTR_FLAG_SHARED. This is the only thing that
// needs to differ from Adafruit_NeoPixel's built-in espShow(): on the ESP32
// classic the RMT peripheral has a single shared IRQ line, and FastAccelStepper
// already owns that line via rmt_isr_register(..., ESP_INTR_FLAG_SHARED, ...).
// Adafruit's espShow() calls rmt_driver_install(ch, 0, 0) → non-shared, which
// fails silently and then rmt_wait_tx_done() blocks forever.
//
// We use a fixed channel (default 4) above the FAS channels (0..3 for the
// 4 UC2_4 steppers) and never uninstall the driver — once initialised, show()
// just feeds new pixel data via rmt_write_sample.
namespace Ws2812Rmt
{
    // One-time initialisation. Must be called BEFORE the first show().
    // Returns true on success. Safe to call multiple times (no-op after first).
    bool begin(int gpioPin, int rmtChannel = 4);

    // Push pixel bytes to the strip. Bytes layout matches what
    // Adafruit_NeoPixel::getPixels() returns (GRB-ordered for NEO_GRB strips).
    void show(const uint8_t *pixels, size_t numBytes);
}
