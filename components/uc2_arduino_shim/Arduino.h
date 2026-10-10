/* Minimal Arduino.h shim for pure-ESP-IDF builds (T1.2).
 *
 * Satisfies #include <Arduino.h> for already-C components (canopen_node) that
 * use the IDF drivers directly. The full Arduino->IDF mapping is uc2_hal (T1.3).
 * Uses only stdio + FreeRTOS (always available) for the helpers. */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
/* SemaphoreHandle_t (canopen_node's CO_CANmodule_t). Not exposed by
 * FreeRTOS.h/task.h under IDF 4.4 (PIO); the real Arduino.h provides it. */
#include "freertos/semphr.h"
/* SemaphoreHandle_t (used by canopen_node's CO_CANmodule_t). The real
 * Arduino.h exposes it via freertos/semphr.h; IDF 4.4 (PIO) does not pull it
 * in via FreeRTOS.h/task.h, so include it explicitly. */
#include "freertos/semphr.h"
/* The real Arduino.h exposes the IDF logging macros (ESP_LOGI, ...). */
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t millis(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }
static inline void delay(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

/* UC2 variadic log helper (no in-repo definition; used by canopen_node + libs). */
static inline void log_i(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    putchar('\n');
    va_end(args);
}

#ifdef __cplusplus
}
#endif
