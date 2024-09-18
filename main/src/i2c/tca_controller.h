#pragma once
#include <PinConfig.h>
#include "Arduino.h"
#include "TCA9555.h"

namespace tca_controller
{

    // TOOD: Make this part of the pinconfig?
    static int pinEnable = 0;
    static int pinDirX = 1;
    static int pinDirY = 2;
    static int pinDirZ = 3;
    static int pinDirA = 4;
    static int pinEndstopX = 5;
    static int pinEndstopY = 6;
    static int pinEndstopZ = 7;

	static TCA9535 TCA(pinConfig.I2C_ADD_TCA);

    bool setExternalPin(uint8_t pin, uint8_t value);
    void init_tca();
    static bool tca_initiated = false;
    bool tca_read_endstop(uint8_t pin);

};