// Host stub: esp_task_wdt.h
#pragma once
#include <cstdint>
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

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t* config);
esp_err_t esp_task_wdt_init(TickType_t timeout_ms, int auto_load);
esp_err_t esp_task_wdt_register(void* task);
esp_err_t esp_task_wdt_register_low_priority(void* task);
esp_err_t esp_task_wdt_deregister(void* task);
esp_err_t esp_task_wdt_feed(void* task);
esp_err_t esp_task_wdt_reset(void);
esp_err_t esp_task_wdt_add_task(void* task, esp_task_wdt_add_arg_t arg);
esp_err_t esp_task_wdt_remove_task(void* task, esp_task_wdt_add_arg_t arg);
esp_err_t esp_task_wdt_enable(void* task);
esp_err_t esp_task_wdt_disable(void* task);
esp_err_t esp_task_wdt_reset_config(void);
