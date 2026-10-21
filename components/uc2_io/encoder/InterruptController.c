#include "InterruptController.h"
#include "PinConfig.h"

// T4.1: pure-C interrupt controller (was a C++ namespace with std::map).
// Listeners live in a fixed array indexed by pin.

static QueueHandle_t dataQueue = NULL;
static ic_listener_t interrupt_listeners[IC_MAX_LISTENERS] = {0};
static bool interrupt_controller_is_init = false;
static const char *TAG = "InterruptController";

// fast returning isr handler that fills up the queue with the pin that triggered
void IRAM_ATTR ic_isr_handler(void *arg)
{
    uint8_t pin = (uint8_t)(uintptr_t)arg;
    xQueueSendToBackFromISR(dataQueue, &pin, NULL);
}

// listen to queue changes and invoke the method attached to pin
static void ic_queue_handler(void *param)
{
    uint8_t pin;
    ESP_LOGI(TAG, "start QueueHandler");
    for (;;)
    {
        // wait for messages in the queue
        if (xQueueReceive(dataQueue, &pin, portMAX_DELAY))
        {
            ESP_LOGD(TAG, "xQueueReceive from pin %i", pin);
            if (pin < IC_MAX_LISTENERS && interrupt_listeners[pin] == NULL)
            {
                // no listener registered for this pin — skip (the old C++
                // version returned here and killed the task; continue is the
                // obvious intent)
                continue;
            }
            // call the function that is bound to the pin
            interrupt_listeners[pin](pin);
        }
    }
}

void interrupt_controller_init(void)
{
    ESP_LOGI(TAG, "init");
    interrupt_controller_is_init = true;
    gpio_install_isr_service(0);
    dataQueue = xQueueCreate(10, sizeof(uint8_t));
    xTaskCreate(
        ic_queue_handler,                                  /* Task function. */
        "HandleData",                                      /* String with name of task. */
        pinConfig.INTERRUPT_CONTROLLER_TASK_STACKSIZE,     /* Stack size in bytes. */
        NULL,                                              /* Parameter passed as input to the task */
        pinConfig.DEFAULT_TASK_PRIORITY,                   /* Priority at which the task is created. */
        NULL);                                             /* Task handle. */
}

void interrupt_controller_add_listener(uint8_t pin, ic_listener_t listener,
                                       gpio_int_type_t int_type)
{
    if (!interrupt_controller_is_init)
        interrupt_controller_init();
    interrupt_listeners[pin] = listener;
    gpio_config_t io_conf_pwm = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = int_type, // To handle CHANGE interrupt type
    };
    gpio_config(&io_conf_pwm);
    gpio_isr_handler_add((gpio_num_t)pin, ic_isr_handler, (void *)(uintptr_t)pin);
}

void interrupt_controller_remove_listener(uint8_t pin)
{
    interrupt_listeners[pin] = NULL;
    gpio_isr_handler_remove((gpio_num_t)pin);
}
