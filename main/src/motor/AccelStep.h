#pragma once
#include "AccelStepper.h"
#include "MotorTypes.h"

void driveMotorXLoop(void *pvParameter);
void driveMotorYLoop(void *pvParameter);
void driveMotorZLoop(void *pvParameter);
void driveMotorALoop(void *pvParameter);

class AccelStep
{
private:
    bool (*_externalCallForPin)(uint8_t pin, uint8_t value);
    std::array<AccelStepper *, 4> steppers;

public:
    std::array<MotorData *, 4> data;
    void setupAccelStepper();
    void startAccelStepper(int i);
    void stopAccelStepper(int i);
    void setExternalCallForPin(bool (*func)(uint8_t pin, uint8_t value));
    void driveMotorLoop(int stepperid);
    void updateData(int i);
    void Enable(bool enable);
    void setPosition(Stepper s, int val);
};