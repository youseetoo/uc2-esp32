#pragma once
#include "FastAccelStepper.h"
#include "FocusMotor.h"

namespace i2cUc2Motor
{

    //static std::array<i2cUc2Stepper *, 4> i2cuc2steppers;

    void starti2cUc2Stepper(int i);
    void stopi2cUc2Stepper(int i);
    void setupi2cUc2Stepper();
    void setupi2cUc2Stepper(Stepper stepper, int motoren, int motordir, int motorstp);
    void setExternalCallForPin(bool (*func)(uint8_t pin, uint8_t value));
    void updateData(int i);
    long getCurrentPosition(Stepper s);
    void setAutoEnable(bool enable);
    void Enable(bool enable);
    void setPosition(Stepper s, int val);
    bool isRunning(int i);
    void move(Stepper s, int steps, bool blocking);
};