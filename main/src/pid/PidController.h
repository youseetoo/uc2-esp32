#include "../../config.h"
#pragma once
#include "ArduinoJson.h"

#include "../wifi/WifiController.h"
#include "../../ModuleController.h"
#include "../analogin/AnalogInPins.h"

namespace RestApi
{
    void Pid_act();
    void Pid_get();
    void Pid_set();
};

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
    AnalogInPins pins;

    int nanaloginavg = 50;

    void setup() override;
    void loop() override;
    void act() override;
    void get() override;
    void set() override;
};
