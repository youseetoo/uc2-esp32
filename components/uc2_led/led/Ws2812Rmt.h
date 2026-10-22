#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Bare-metal WS2812 (NeoPixel) driver for the ESP32 RMT peripheral.
//
// T4.1: converted to pure C (was the Ws2812Rmt namespace); the only caller
// is the LedController C++ island, which uses the C API directly.
//
// Supported chips:
//   ESP32-classic — channels 0..7, 64 items/block.  Default channel: 4
//                   (FAS occupies 0..3 on UC2_4).
//   ESP32-S3      — TX channels 0..3, 48 items/block.  Default channel: 0
//                   (waveshare_esp32s3_ledarray has no motors).
//
// On boards with motors the caller must pass a channel number that is not
// used by FastAccelStepper.  Override via -DUC2_WS2812_RMT_CHANNEL=N.

#ifdef __cplusplus
extern "C" {
#endif

// One-time initialisation.  Must be called before the first show().
// Returns true on success.  Safe to call multiple times (no-op after first).
// rmtChannel must be 0..3 on S3 (TX channels only), 0..7 on classic ESP32.
bool ws2812_rmt_begin(int gpioPin, int rmtChannel);

// Push pixel bytes to the strip.  Layout matches Adafruit_NeoPixel::getPixels()
// (GRB-ordered for NEO_GRB strips).  Streams the frame in RMT-sized chunks
// with interrupts disabled on the calling core, and blocks until the strip
// has been clocked out (~30 us per pixel, e.g. ~2 ms for 64 LEDs).
void ws2812_rmt_show(const uint8_t *pixels, size_t numBytes);

#ifdef __cplusplus
}
#endif
