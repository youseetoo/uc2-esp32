#pragma once
#include "FastAccelStepper.h"
#include "FocusMotor.h"

#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif
namespace FAccelStep
{

    static FastAccelStepperEngine engine = FastAccelStepperEngine();
    static std::array<FastAccelStepper *, 4> faststeppers;
    static bool (*_externalCallForPin)(uint8_t pin, uint8_t value);
    static Preferences preferences;
    void startFastAccelStepper(int i);
    void stopFastAccelStepper(int i);
    void setupFastAccelStepper();
    void setupFastAccelStepper(Stepper stepper, int motoren, int motordir, int motorstp);
    void setExternalCallForPin(bool (*func)(uint8_t pin, uint8_t value));
    void updateData(int i);
    long getCurrentPosition(Stepper s);
    void setAutoEnable(bool enable);
    void Enable(bool enable);
    void setPosition(Stepper s, int val);
    bool isRunning(int i);
    void move(Stepper s, int steps, bool blocking);
    void setSoftLimits(int axis, int32_t minPos, int32_t maxPos);

    // store old motor current for each motor
    static int oldMotorCurrent[4] = {0, 0, 0, 0};

};