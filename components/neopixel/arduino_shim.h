// T5.3: Arduino API shim for raw ESP-IDF (NeoPixel leaf, until full conversion).
// Provides the small set of Arduino functions Adafruit_NeoPixel.{h,cpp} uses.
// Time functions (micros/millis/delay/delayMicroseconds) come from the
// project's uc2_time.h (the Arduino-time drop-in) — NOT redefined here.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>            // malloc, free
#include <cctype>              // tolower
#include "esp_idf_version.h"   // ESP_IDF_VERSION, ESP_IDF_VERSION_VAL
#include "driver/gpio.h"       // gpio_set_direction, gpio_set_level
#include "uc2_hal/uc2_time.h"  // micros/millis/delay/delayMicroseconds

#ifndef INPUT
#define INPUT GPIO_MODE_INPUT
#endif
#ifndef OUTPUT
#define OUTPUT GPIO_MODE_OUTPUT
#endif
#ifndef LOW
#define LOW 0
#endif
#ifndef HIGH
#define HIGH 1
#endif

static inline void pinMode(int pin, int mode) {
    gpio_set_direction((gpio_num_t)pin, (gpio_mode_t)mode);
}
static inline void digitalWrite(int pin, int value) {
    gpio_set_level((gpio_num_t)pin, value ? 1 : 0);
}
