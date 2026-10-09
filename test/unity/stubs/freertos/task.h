// Host stub: freertos/task.h — single-threaded; tasks run inline or as no-ops.
#pragma once
#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void*);

BaseType_t xTaskCreate(TaskFunction_t fn, const char* name, uint32_t stack,
                       void* param, UBaseType_t prio, TaskHandle_t* handle);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char* name,
                                   uint32_t stack, void* param, UBaseType_t prio,
                                   TaskHandle_t* handle, int core);
void vTaskDelete(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);
void vTaskDelayUntil(TickType_t* lastWake, TickType_t inc);
void vTaskSuspend(TaskHandle_t handle);
void vTaskResume(TaskHandle_t handle);
void vTaskSuspendAll(void);
BaseType_t xTaskResumeAll(void);
void taskYIELD(void);
TickType_t xTaskGetTickCount(void);
uint32_t xTaskGetTickCountFromISR(void);
UBaseType_t uxTaskGetNumberOfTasks(void);
