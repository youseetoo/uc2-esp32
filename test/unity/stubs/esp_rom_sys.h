// Host stub: esp_rom_sys.h
//
// esp_rom_delay_us is the 6.1 replacement for esp_delay_us (busy-wait).
// On the host it is NOT a real busy-wait: it advances the process-wide fake
// clock (host_fake_clock_advance_us in host_impl.cpp) so that
// delayMicroseconds() from uc2_hal/uc2_time.h behaves deterministically and
// stays consistent with Arduino delay()/millis() in the same binary.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Advances the host fake clock by us (defined in host_impl.cpp).
void host_fake_clock_advance_us(uint32_t us);

#ifdef __cplusplus
}
#endif

static inline void esp_rom_delay_us(uint32_t us)
{
    host_fake_clock_advance_us(us);
}
