// Host stub: esp_system.h
#pragma once
#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif
#include "esp_err.h"

typedef int esp_reset_reason_t;
#define ESP_RST_POWERON 0
#define ESP_RST_EXT 1
#define ESP_RST_SW 2
#define ESP_RST_PANIC 3
#define ESP_RST_INT_WDT 4
#define ESP_RST_TASK_WDT 5
#define ESP_RST_WDT 6
#define ESP_RST_DEEP_SLEEP 7
#define ESP_RST_BROWNOUT 8
#define ESP_RST_SDIO 9

// extern "C" guard: C TUs (State.c esp_restart etc.) and C++ TUs must see the
// same linkage (definitions in host_impl.cpp, guarded to match).
#ifdef __cplusplus
extern "C" {
#endif

void esp_restart(void);
uint32_t esp_get_free_heap_size(void);
uint32_t esp_get_free_internal_heap_size(void);
uint32_t esp_get_minimum_free_heap_size(void);
uint32_t esp_get_free_heap_size_static(void);
// esp_random is declared by esp_random.h (static inline LCG). A C-linkage
// declaration here collided with that inline in C++ TUs (C2732).
esp_reset_reason_t esp_reset_reason(void);
uint32_t esp_sntp_get_time(void);
void ets_delay_us(uint32_t us);  // ESP-IDF microsecond busy-wait (no-op on host)

#ifdef __cplusplus
}
#endif

// No Arduino `ESP` global here: uc2_esp.h (T1.3) declares
// `extern const EspClass ESP` and is always visible via uc2_hal.h, so a second
// ESP declaration collided (C2373). components/uc2_hal/src/uc2_esp.cpp is the
// single definition (added to the host test build's prodSrcs).
