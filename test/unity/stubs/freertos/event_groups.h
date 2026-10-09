// Host stub: freertos/event_groups.h
#pragma once
#include "FreeRTOS.h"

EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupSetBits(EventGroupHandle_t g, EventBits_t bits);
EventBits_t xEventGroupClearBits(EventGroupHandle_t g, EventBits_t bits);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t g, EventBits_t bits, BaseType_t clear,
                                BaseType_t any, TickType_t timeout);
EventBits_t xEventGroupGetBits(EventGroupHandle_t g);
void vEventGroupDelete(EventGroupHandle_t g);
