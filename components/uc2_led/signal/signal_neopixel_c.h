// T4.1: C shim for the C++ Adafruit_NeoPixel strip — used by the pure-C
// SignalController. The C module calls these C functions; the implementation
// (signal_neopixel_c.cpp, a thin C++ TU) forwards to the C++ Adafruit_NeoPixel
// class. Keeps the C module free of C++ dependencies.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Create a NEO_GRB / 800 kHz strip of `numPixels` pixels on `pin`
// (the only configuration SignalController uses). Returns an opaque handle,
// or NULL on allocation failure.
void *signal_strip_create(uint16_t numPixels, int pin);
void signal_strip_destroy(void *strip);
void signal_strip_begin(void *strip);
void signal_strip_set_brightness(void *strip, uint8_t b);
void signal_strip_clear(void *strip);
void signal_strip_show(void *strip);
uint8_t signal_strip_gamma8(void *strip, uint8_t v);
uint32_t signal_strip_color(void *strip, uint8_t r, uint8_t g, uint8_t b);
void signal_strip_set_pixel(void *strip, uint16_t idx, uint32_t c);

#ifdef __cplusplus
}
#endif
