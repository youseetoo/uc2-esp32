#pragma once

#include "../../ModuleController.h"
#include "../motor/FocusMotor.h"


class PidController : public Module
{
private:
    /* data */
    long returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd);
public:
    PidController(/* args */);
    ~PidController();
    bool DEBUG = false;

    float errorRunSum=0;
    float previousError=0;
    float stepperMaxValue=2500.;
    float PID_Kp = 10;
    float PID_Ki = 0.1;
    float PID_Kd = 0.1;
    float PID_target = 500;
    float PID_updaterate = 200; // ms
    bool PID_active=false;
    // timing variables
	unsigned long startMillis;
	unsigned long currentMillis;

    int N_analogin_avg = 50;

    void setup() override;
    void loop() override;
    int act(cJSON * ob) override;
    cJSON * get(cJSON * ob) override;
};
