#include "InterruptController.h"
#include "PinConfig.h"

namespace InterruptController{
    
    // pins that got interrupted and returned from isr
    QueueHandle_t dataQueue = xQueueCreate(10, sizeof(uint8_t));
    // maps functions pointers that listen to a specific pin
    std::map<uint8_t, Listner> interruptListners;
    bool interruptControllerIsInit = false;
    static const char *TAG = "InterruptController";

    // fast returning isr handler that fills up the queue with the pin that triggert
    void IRAM_ATTR ISR_handler(void *arg)
    {
        int pin = (int)arg;
        xQueueSendToBackFromISR(dataQueue, &pin, NULL);
    };

    // listen to queue changes and invoke the method attached to pin
    void QueueHandler(void *param)
    {
        ESP_LOGI(TAG, "start QueueHandler");
        uint8_t pin;
        for (;;)
        {

            // wait for messages int the queue
            if (xQueueReceive(dataQueue, &pin, portMAX_DELAY))
            {
                ESP_LOGD(TAG, "xQueueReceive from pin %i", pin);
                if (interruptListners[pin] == nullptr)
                    return;
                // call the function that is bond to the pin
                interruptListners[pin](pin);
            }
            // vTaskDelay(1);
        }
        ESP_LOGI(TAG, "end QueueHandler");
    };

     void init()
    {
        ESP_LOGI(TAG, "init");
        interruptControllerIsInit = true;
        gpio_install_isr_service(0);
        xTaskCreate(
            QueueHandler,                                  /* Task function. */
            "HandleData",                                  /* String with name of task. */
            pinConfig.INTERRUPT_CONTROLLER_TASK_STACKSIZE, /* Stack size in bytes. */
            NULL,                                          /* Parameter passed as input to the task */
            pinConfig.DEFAULT_TASK_PRIORITY,               /* Priority at which the task is created. */
            NULL);                                         /* Task handle. */
    };

    void addInterruptListner(uint8_t pin, Listner listner, gpio_int_type_t int_type)
    {
        if (!interruptControllerIsInit)
            init();
        interruptListners.insert(std::make_pair(pin, listner));
        gpio_config_t io_conf_pwm = {
            .pin_bit_mask = (1ULL << pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = int_type, // To handle CHANGE interrupt type
        };
        gpio_config(&io_conf_pwm);
        gpio_isr_handler_add((gpio_num_t)pin, ISR_handler, (void *)pin);
    };

    void removeInterruptListner(uint8_t pin)
    {
        interruptListners.erase(pin);
        gpio_isr_handler_remove((gpio_num_t)pin);
    };
}