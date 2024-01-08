#include "InterruptController.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "PinConfig.h"

// Global variable definitions
QueueHandle_t dataQueue = xQueueCreate(10, sizeof(uint8_t));
std::map<uint8_t, Listener> interruptListeners;
const char *InterruptTAG = "InterruptController"; // Definition

bool interruptControllerIsInit = false;

// Function definitions
void IRAM_ATTR ISR_handler(void *arg) {
    int pin = (int)arg;
    xQueueSendToBackFromISR(dataQueue, &pin, NULL);
}

void QueueHandler(void *param) {
    ESP_LOGI(InterruptTAG, "start QueueHandler");
    uint8_t pin;
    for (;;) {
        if (xQueueReceive(dataQueue, &pin, portMAX_DELAY)) {
            ESP_LOGD(InterruptTAG, "<<<xQueueReceive from pin>>> %i", pin);
            if (interruptListeners[pin] == nullptr)
                return;
            interruptListeners[pin](pin);
        }
    }
    ESP_LOGI(InterruptTAG, "end QueueHandler");
}

void init() {
    ESP_LOGI(InterruptTAG, "init");
    interruptControllerIsInit = true;
    gpio_install_isr_service(0);
    xTaskCreate(
        QueueHandler, /* Task function. */
        "HandleData", /* String with name of task. */
        pinConfig.INTERRUPT_TASK_STACKSIZE,        /* Stack size in bytes. */
        NULL,         /* Parameter passed as input to the task */
        1,            /* Priority at which the task is created. */
        NULL);        /* Task handle. */
}

void addInterruptListener(uint8_t pin, Listener listener, gpio_int_type_t int_type) {
    log_d("addInterruptListener %i", pin);
    if (!interruptControllerIsInit)
        init();
    interruptListeners.insert(std::make_pair(pin, listener));
    gpio_config_t io_conf_pwm = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = int_type,
    };
    gpio_config(&io_conf_pwm);
    gpio_isr_handler_add((gpio_num_t)pin, ISR_handler, (void *)pin);
}

void removeInterruptListener(uint8_t pin) {
    interruptListeners.erase(pin);
    gpio_isr_handler_remove((gpio_num_t)pin);
}
