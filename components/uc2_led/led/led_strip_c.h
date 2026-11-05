// led_strip_c.h — C API for LED strip drivers (T4.1/T5.3).
//
// The driver is selected at build time:
//   - DOTSTAR: Adafruit_DotStar (C++ wrapper in led_strip_c.cpp)
//   - HUB75:   Adafruit_Protomatter (C++ wrapper in led_strip_c.cpp)
//   - WS2812:  Pure C RMT driver (ws2812_rmt.c), no Adafruit dependency
//
// The Adafruit_NeoPixel dependency has been replaced with the ws2812_rmt
// bare-metal C driver for the WS2812 path. The C++ shim (led_strip_c.cpp)
// still wraps the Adafruit C++ classes for DotStar and HUB75 (deferred to T5.x).
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Create an LED strip object. Returns opaque handle.
void *led_strip_create(void);

// Initialize the strip hardware. Returns true on success.
bool led_strip_begin(void *handle);

// Update the LEDs with the current pixel data.
void led_strip_show(void *handle);

// Set a pixel's color. Index 0 is the first pixel.
// Color is 32-bit RGB: 0xRRGGBB (alpha ignored).
void led_strip_set_pixel(void *handle, uint16_t index, uint32_t color);

// Clear all pixels to black.
void led_strip_clear(void *handle);

// Fill all pixels with a single color.
void led_strip_fill(void *handle, uint32_t color);

// Set strip brightness (0-255).
void led_strip_set_brightness(void *handle, uint8_t brightness);

// Create a 32-bit RGB color value (little-endian: 0xRRGGBB).
uint32_t led_strip_color(uint8_t r, uint8_t g, uint8_t b);

// Create a 32-bit RGB color from HSV (0-65535 hue, 0-255 sat/val).
uint32_t led_strip_color_hsv(uint16_t hue, uint8_t sat, uint8_t val);

// Gamma-correct a 32-bit color value.
uint32_t led_strip_gamma32(uint32_t color);

// HUB75-only: create 16-bit 565 color value.
uint16_t led_strip_color565(void *handle, uint8_t r, uint8_t g, uint8_t b);

// HUB75-only: draw a pixel at (x, y) with 16-bit 565 color.
void led_strip_draw_pixel(void *handle, int x, int y, uint16_t color565);

// Get pointer to raw pixel buffer (for advanced use).
uint8_t *led_strip_pixels(void *handle);

// Get the number of pixels in the strip.
uint16_t led_strip_num_pixels(void *handle);

#ifdef __cplusplus
}
#endif