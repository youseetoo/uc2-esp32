#pragma once
#include "i2c_master.h"
namespace i2c_slave_motor
{
    void setup();
    void loop();

    void setI2CAddress(int address);
    int getI2CAddress();

    static I2C_REQUESTS currentRequest = I2C_REQUESTS::REQUEST_MOTORSTATE;
};
