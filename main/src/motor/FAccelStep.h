#pragma once
#include "FastAccelStepper.h"
#include "FocusMotor.h"

namespace FAccelStep
{

    static FastAccelStepperEngine engine = FastAccelStepperEngine();
    static std::array<FastAccelStepper *, 4> faststeppers;
    static bool (*_externalCallForPin)(uint8_t pin, uint8_t value);
    
    void startFastAccelStepper(int i);
    void stopFastAccelStepper(int i);
    void setupFastAccelStepper();
    void setupFastAccelStepper(Stepper stepper, int motoren, int motordir, int motorstp);
    void setExternalCallForPin(bool (*func)(uint8_t pin, uint8_t value));
    void updateData(int i);
    void setAutoEnable(bool enable);
    void Enable(bool enable);
    void setPosition(Stepper s, int val);
    bool isRunning(int i);
    void move(Stepper s, int steps, bool blocking);
};