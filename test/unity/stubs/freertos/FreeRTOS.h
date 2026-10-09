// Host stub: freertos/FreeRTOS.h
#pragma once
#include <cstdint>

typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef void* EventGroupHandle_t;
typedef uint32_t EventBits_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define pdTICKS_TO_MS(x) ((TickType_t)(x))
#define portMAX_DELAY ((TickType_t)0xFFFFFFFF)
#define portTICK_PERIOD_MS ((TickType_t)10)
#define configTICK_RATE_HZ 1000

// ESP32 port critical-section mutex (spinlock-backed on target; trivial on host).
typedef struct { int32_t lock; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED { 0 }
#define portMUX_INITIALIZER_LOCKED   { 1 }

// Critical-section entry/exit (take a portMUX_TYPE*). Single-threaded on host,
// so these are no-ops; the signature matches the ESP32 port.
static inline void portENTER_CRITICAL(portMUX_TYPE* mux) { (void)mux; }
static inline void portEXIT_CRITICAL(portMUX_TYPE* mux) { (void)mux; }

// Component headers — real FreeRTOS.h includes all of these, so any file that
// includes FreeRTOS.h gets TaskHandle_t/SemaphoreHandle_t/xQueue*/xEventGroup*.
// #pragma once in each header makes the circular FreeRTOS.h re-include safe; the
// base types above are already defined by the time they are needed.
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "event_groups.h"
