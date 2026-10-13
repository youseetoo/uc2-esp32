#pragma once
// T2.1: Arduino pgmspace.h shim for raw IDF. PROGMEM is a no-op on ESP32 (SRAM,
// no separate program space). Provided so headers that #include <pgmspace.h>
// (e.g. uc2_app/JsonKeys.h, neopixel/Adafruit_NeoPixel.h) resolve without Arduino.

#ifndef PROGMEM
#define PROGMEM
#endif

#include <stdint.h>
// Arduino pgm_read_* — on ESP32 the "program space" is just RAM, so a plain read.
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif
