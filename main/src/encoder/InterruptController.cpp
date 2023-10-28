#include "InterruptController.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Global variable definitions
QueueHandle_t dataQueue = xQueueCreate(10, sizeof(uint8_t));
std::map<uint8_t, Listner> interruptListners;
bool interruptControllerIsInit = false;

// Function definitions
void IRAM_ATTR ISR_handler(void *arg) {
    int pin = (int)arg;
    xQueueSendToBackFromISR(dataQueue, &pin, NULL);
}

void QueueHandler(void *param) {
    ESP_LOGI(TAG, "start QueueHandler");
    uint8_t pin;
    for (;;) {
        if (xQueueReceive(dataQueue, &pin, portMAX_DELAY)) {
            ESP_LOGD(TAG, "xQueueReceive from pin %i", pin);
            if (interruptListners[pin] == nullptr)
                return;
            interruptListners[pin](pin);
        }
    }
    ESP_LOGI(TAG, "end QueueHandler");
}

void init() {
    ESP_LOGI(TAG, "init");
    interruptControllerIsInit = true;
    gpio_install_isr_service(0);
    xTaskCreate(
        QueueHandler, /* Task function. */
        "HandleData", /* String with name of task. */
        8096,        /* Stack size in bytes. */
        NULL,         /* Parameter passed as input to the task */
        1,            /* Priority at which the task is created. */
        NULL);        /* Task handle. */
}

void addInterruptListner(uint8_t pin, Listner listner, gpio_int_type_t int_type) {
    if (!interruptControllerIsInit)
        init();
    interruptListners.insert(std::make_pair(pin, listner));
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

void removeInterruptListner(uint8_t pin) {
    interruptListners.erase(pin);
    gpio_isr_handler_remove((gpio_num_t)pin);
}
