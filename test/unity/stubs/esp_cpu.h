// Host stub: esp_cpu.h
//
// esp_cpu_get_cycle_count must return CPU CYCLES, not µs (Ws2812Rmt compares
// against ~480000 ≈ 2 ms @ 240 MHz — see uc2_hal/uc2_esp.h). The host maps
// the fake clock at a fixed 240 MHz so the cycle counts stay consistent.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t esp_cpu_get_cycle_count(void);

#ifdef __cplusplus
}
#endif
