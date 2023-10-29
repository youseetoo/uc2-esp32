#pragma once

#include <map>
#include <Arduino.h>
#include "freertos/queue.h"

// Function layout that expects uint8_t as an argument
typedef void (*Listener)(uint8_t pin);

// Global variable declarations
extern const char *InterruptTAG; // Declaration only

extern QueueHandle_t dataQueue;
extern std::map<uint8_t, Listener> interruptListeners;
extern bool interruptControllerIsInit;

// Function declarations
void IRAM_ATTR ISR_handler(void *arg);
void QueueHandler(void *param);
void init();
void addInterruptListener(uint8_t pin, Listener listener, gpio_int_type_t int_type);
void removeInterruptListener(uint8_t pin);
