#pragma once
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
// T1.3: direct IDF (no Arduino) — gpio_int_type_t comes from the IDF gpio
// driver (same header uc2_gpio.cpp uses).
#include <driver/gpio.h>


namespace InterruptController
{
    // function layout that expect uint8_t as arg
    typedef void (*Listner)(uint8_t pin);
    void init();
    void addInterruptListner(uint8_t pin, Listner listner, gpio_int_type_t int_type);
    void removeInterruptListner(uint8_t pin);
};
