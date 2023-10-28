#pragma once

#include <map>
#include <Arduino.h>
#include "freertos/queue.h"

// Function layout that expects uint8_t as an argument
typedef void (*Listner)(uint8_t pin);

// Global variable declarations
extern const char *TAG = "InterruptController";
extern QueueHandle_t dataQueue;
extern std::map<uint8_t, Listner> interruptListners;
extern bool interruptControllerIsInit;

// Function declarations
void IRAM_ATTR ISR_handler(void *arg);
void QueueHandler(void *param);
void init();
void addInterruptListner(uint8_t pin, Listner listner, gpio_int_type_t int_type);
void removeInterruptListner(uint8_t pin);
