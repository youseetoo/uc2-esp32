// Host stub: esp_task_wdt.h
#pragma once
#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

typedef enum {
    TWDAQ_ANY = 0,
    TWDAQ_CORE0,
    TWDAQ_CORE1
} esp_task_wdt_add_arg_t;

typedef enum {
    TWDOF_STALL = 1 << 0,
    TWDOF_ISR = 1 << 1
} esp_task_wdt_config_flags_t;

typedef struct {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    uint32_t slow_down_core_mask;
    uint32_t flags;
} esp_task_wdt_config_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t* config);
esp_err_t esp_task_wdt_register(void* task);
esp_err_t esp_task_wdt_register_low_priority(void* task);
esp_err_t esp_task_wdt_deregister(void* task);
esp_err_t esp_task_wdt_feed(void* task);
esp_err_t esp_task_wdt_reset(void);
// IDF 5.x legacy one-arg form (HighSpeedScannerCore.cpp: esp_task_wdt_add(NULL)).
esp_err_t esp_task_wdt_add(void* task);
esp_err_t esp_task_wdt_remove(void* task);
esp_err_t esp_task_wdt_add_task(void* task, esp_task_wdt_add_arg_t arg);
esp_err_t esp_task_wdt_remove_task(void* task, esp_task_wdt_add_arg_t arg);
esp_err_t esp_task_wdt_enable(void* task);
esp_err_t esp_task_wdt_disable(void* task);
esp_err_t esp_task_wdt_reset_config(void);

#ifdef __cplusplus
}
#endif

// Legacy 2-arg form (main.cpp). C++-only overload: C linkage forbids
// overloading (C2733); no C component TU calls the 2-arg form.
#ifdef __cplusplus
esp_err_t esp_task_wdt_init(TickType_t timeout_ms, int auto_load);
#endif
