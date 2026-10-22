// Host stub: esp_timer.h
//
// esp_timer_get_time returns the host fake clock (process-wide, in µs), the
// SAME clock that Arduino millis()/micros()/delay() in host_impl.cpp use.
// That keeps the C-side time API (uc2_hal/uc2_time.h, which maps millis()/
// micros() onto esp_timer_get_time) consistent with the C++ side in one binary.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reads the host fake clock in µs (defined in host_impl.cpp).
uint64_t host_fake_clock_us(void);

#ifdef __cplusplus
}
#endif

// Monotonic µs since the start of the host process (fake clock).
static inline int64_t esp_timer_get_time(void)
{
    return (int64_t)host_fake_clock_us();
}
