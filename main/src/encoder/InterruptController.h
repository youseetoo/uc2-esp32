#pragma once
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <Arduino.h>


namespace InterruptController
{
    // function layout that expect uint8_t as arg
    typedef void (*Listner)(uint8_t pin);
    void init();
    void addInterruptListner(uint8_t pin, Listner listner, gpio_int_type_t int_type);
    void removeInterruptListner(uint8_t pin);
};
