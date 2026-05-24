#pragma once
#include <stdint.h>
#include <stddef.h>

// Bare-metal WS2812 (NeoPixel) driver for the ESP32 RMT peripheral.
//
// Supported chips:
//   ESP32-classic — channels 0..7, 64 items/block.  Default channel: 4
//                   (FAS occupies 0..3 on UC2_4).
//   ESP32-S3      — TX channels 0..3, 48 items/block.  Default channel: 0
//                   (waveshare_esp32s3_ledarray has no motors).
//
// On boards with motors the caller must pass a channel number that is not
// used by FastAccelStepper.  Override via -DUC2_WS2812_RMT_CHANNEL=N.
namespace Ws2812Rmt
{
    // One-time initialisation.  Must be called before the first show().
    // Returns true on success.  Safe to call multiple times (no-op after first).
    // rmtChannel must be 0..3 on S3 (TX channels only), 0..7 on classic ESP32.
    bool begin(int gpioPin, int rmtChannel = 4);

    // Push pixel bytes to the strip.  Layout matches Adafruit_NeoPixel::getPixels()
    // (GRB-ordered for NEO_GRB strips).
    void show(const uint8_t *pixels, size_t numBytes);
}
