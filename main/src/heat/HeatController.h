#pragma once

#include "DS18b20Controller.h"
#include "../laser/LaserController.h"
#include <Preferences.h>

namespace HeatController
{
    /* data */
    long returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd);

    static Preferences hPreferences;

    static float errorRunSum=0;
    static float previousError=0;
    static int pwm_max_value = 1023;
    static int pwm_min_value = 0;
    static float temp_pid_Kp = 10;
    static float temp_pid_Ki = 0.1;
    static float temp_pid_Kd = 0.1;
    static float temp_pid_target = 500;
    static float temp_pid_updaterate = 200000; // ms
    static int maxPWMValue = 511;
    static bool Heat_active=false;

    static long t_tempControlStarted = 0;
    static float temp_tempControlStarted = 0.0f;
    static int timeToReach80PercentTargetTemperature = 200000; // change in temperature in 60 seconds otherwise we stop

    // timing variables
	static unsigned long startMillis;
	static unsigned long currentMillis;

    static int N_analogin_avg = 50;

    void setup();
    void loop();
    int act(cJSON * ob);
    cJSON * get(cJSON * ob);
};
