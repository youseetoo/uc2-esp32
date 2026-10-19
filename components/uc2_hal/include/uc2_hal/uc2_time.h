// T1.3 (no Arduino): time API over esp_timer / FreeRTOS.
// Drop-in for the Arduino time calls used in main/ (millis 152, micros 10,
// delay 53, delayMicroseconds 8 — grep-verified 2026-10-11).
#pragma once

// C-compatible (stdint, not cstdint): included from C modules (T4.1 State.c).
#include <stdint.h>

#include "esp_rom_sys.h"        // esp_rom_delay_us (6.1: esp_delay_us removed)
#include "esp_timer.h"          // esp_timer_get_time (monotonic µs)
#include "freertos/FreeRTOS.h"  // pdMS_TO_TICKS
#include "freertos/task.h"      // vTaskDelay

// C-compatible: C casts + `static inline` so the header works from both C
// (T4.1 State.c) and C++ TUs.
//
// Milliseconds since boot; wraps at ~49.7 days (Arduino millis() semantics —
// the firmware does (int32_t)(now - then) difference arithmetic).
static inline uint32_t millis(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static inline uint32_t micros(void)
{
    return (uint32_t)(esp_timer_get_time());
}

// Yield for ms — task context only.
static inline void delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// Busy-wait for µs (ISR-safe, like Arduino delayMicroseconds).
static inline void delayMicroseconds(uint32_t us)
{
    esp_rom_delay_us(us);
}
