#pragma once
#include "PinConfig.h"
#include "FastAccelStepper.h"
#include "FocusMotor.h"
#include "../i2c/i2c_controller.h"

namespace i2cUc2Motor
{
    // for A,X,Y,Z intialize the I2C addresses
    uint8_t addresses[] = {
        pinConfig.I2C_ADD_MOT_A,
        pinConfig.I2C_ADD_MOT_X,
        pinConfig.I2C_ADD_MOT_Y,
        pinConfig.I2C_ADD_MOT_Z
    };
    //static std::array<i2cUc2Stepper *, 4> i2cuc2steppers;
    int axis2address(int axis);
    void sendJsonString(String jsonString, uint8_t slave_addr); 
    void starti2cUc2Stepper(int i);
    void stopi2cUc2Stepper(int i);
    void setupi2cUc2Stepper();
    void setExternalCallForPin(bool (*func)(uint8_t pin, uint8_t value));
    void updateData(int i);
    long getCurrentPosition(Stepper s);
    void setAutoEnable(bool enable, int axis);
    void Enable(bool enable, int axis);
    void setPosition(Stepper s, int val);
    bool isRunning(int i);
    void move(Stepper s, int steps, bool blocking);
};