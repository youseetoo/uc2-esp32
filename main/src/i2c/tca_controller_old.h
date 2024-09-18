#pragma once
#include "Arduino.h"
#include "TCA9555.h"

namespace tca_controller
{
    bool setExternalPin(uint8_t pin, uint8_t value);
    void init_tca();
    static bool tca_initiated = false;
    bool tca_read_endstop(uint8_t pin);

};