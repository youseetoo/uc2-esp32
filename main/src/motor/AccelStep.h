#pragma once
#include "AccelStepper.h"
#include "MotorTypes.h"

void driveMotorXLoop(void *pvParameter);
void driveMotorYLoop(void *pvParameter);
void driveMotorZLoop(void *pvParameter);
void driveMotorALoop(void *pvParameter);

namespace AccelStep
{
    static bool (*_externalCallForPin)(uint8_t pin, uint8_t value);
    static std::array<AccelStepper *, 4> steppers;
    static std::array<bool ,4> taskRunning;

    void setupAccelStepper();
    void startAccelStepper(int i);
    void setPosition(Stepper axis, int pos);
    void stopAccelStepper(int i);
    void setExternalCallForPin(bool (*func)(uint8_t pin, uint8_t value));
    void driveMotorLoop(int stepperid);
    void updateData(int i);
    long getCurrentPosition(int i);
    void Enable(bool enable);
    void setPosition(Stepper s, int val);
    bool isRunning(int i);
    void sendMotorPos(int i, int arraypos);
};