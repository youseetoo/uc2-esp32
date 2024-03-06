#pragma once

#include "../../ModuleController.h"
#include "../temp/DS18b20Controller.h"
#include "../laser/LaserController.h"


class HeatController : public Module
{
private:
    /* data */
    long returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd);
public:
    HeatController(/* args */);
    ~HeatController();
    bool DEBUG = false;

    float errorRunSum=0;
    float previousError=0;
    int pwm_max_value = 1023;
    int pwm_min_value = 0;
    float temp_pid_Kp = 10;
    float temp_pid_Ki = 0.1;
    float temp_pid_Kd = 0.1;
    float temp_pid_target = 500;
    float temp_pid_updaterate = 200000; // ms
    int maxPWMValue = 511;
    bool Heat_active=false;

    long t_tempControlStarted = 0;
    float temp_tempControlStarted = 0.0f;
    int timeToReach80PercentTargetTemperature = 200000; // change in temperature in 60 seconds otherwise we stop

    // timing variables
	unsigned long startMillis;
	unsigned long currentMillis;

    int N_analogin_avg = 50;

    void setup() override;
    void loop() override;
    int act(cJSON * ob) override;
    cJSON * get(cJSON * ob) override;
};
