#include "../../config.h"
#ifdef IS_PID
#pragma once
#include "ArduinoJson.h"
#include "../../pinstruct.h"
#include "../wifi/WifiController.h"
#include "../../ModuleController.h"
#include "../sensor/SensorPins.h"

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
    long returnControlValue(float controlTarget, float sensorValue, float Kp, float Ki, float Kd);
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
    SensorPins pins;

    int N_sensor_avg = 50;

    void setup() override;
    void loop() override;
    void act() override;
    void get() override;
    void set() override;
};
#endif
