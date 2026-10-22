// Host stub: freertos/semphr.h
#pragma once
#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
// Legacy FreeRTOS name (FocusMotor.cpp uses xSemaphoreHandle).
typedef SemaphoreHandle_t xSemaphoreHandle;

// extern "C" guard: C TUs (FocusMotor.c semaphores, queues) and C++ TUs must
// see the same linkage (definitions in host_impl.cpp, guarded to match).
#ifdef __cplusplus
extern "C" {
#endif

SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t maxCount, UBaseType_t initCount);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t sem, BaseType_t* woken);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t* woken);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t sem, TickType_t timeout);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t sem);
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t sem);
void vSemaphoreDelete(SemaphoreHandle_t sem);

// Queues — minimal (used by some drivers).
QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize);
BaseType_t xQueueSendToFront(QueueHandle_t q, const void* item, TickType_t timeout);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q);
BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t timeout);
BaseType_t xQueueSendToBackFromISR(QueueHandle_t q, const void* item, BaseType_t* woken);
BaseType_t xQueueReceive(QueueHandle_t q, void* item, TickType_t timeout);
void vQueueDelete(QueueHandle_t q);

#ifdef __cplusplus
}
#endif
