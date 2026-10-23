// led_strip_c.h — C-facing shim over the Adafruit display libraries (T4.1 / PORT_PLAN_STEPS.md).
//
// The strip variant is selected at build time, exactly as before:
//   DOTSTAR        -> Adafruit_DotStar (BGR, data+clk)
//   HUB75          -> Adafruit_Protomatter (matrix, 565 colour)
//   (neither)      -> Adafruit_NeoPixel (WS2812 GRB)
// The implementation (led_strip_c.cpp) is the ONLY file in the component that
// #includes Adafruit headers; C code (LedController.c, tests) uses this API.
//
// `handle` is an opaque pointer to the Adafruit object (NULL on alloc failure).
#ifndef LED_STRIP_C_H
#define LED_STRIP_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Create the configured strip (reads pinConfig for count/pins).
// HUB75 variant: returns NULL if Protomatter alloc fails (caller halts, as before).
void *led_strip_create(void);

// Start the strip. NeoPixel + USE_FASTACCEL: begins the RMT driver instead of
// the library begin(). Returns false on begin failure (HUB75 / RMT).
bool led_strip_begin(void *handle);

// Flush pixels. NeoPixel + USE_FASTACCEL routes through ws2812_rmt_show().
void led_strip_show(void *handle);

void led_strip_set_pixel(void *handle, uint16_t index, uint32_t color);
void led_strip_clear(void *handle);      // HUB75: fillScreen(0)
void led_strip_fill(void *handle, uint32_t color); // HUB75: fillScreen(color)
void led_strip_set_brightness(void *handle, uint8_t brightness);

// Color helpers (static on all three libraries).
uint32_t led_strip_color(uint8_t r, uint8_t g, uint8_t b);
uint32_t led_strip_color_hsv(uint16_t hue, uint8_t sat, uint8_t val);
uint32_t led_strip_gamma32(uint32_t color);
uint16_t led_strip_color565(void *handle, uint8_t r, uint8_t g, uint8_t b); // HUB75 only
void led_strip_draw_pixel(void *handle, int x, int y, uint16_t color565); // HUB75 only

// Pixel buffer / count (used by the RMT show path).
uint8_t *led_strip_pixels(void *handle);
uint16_t led_strip_num_pixels(void *handle);

#ifdef __cplusplus
}
#endif

#endif // LED_STRIP_C_H
