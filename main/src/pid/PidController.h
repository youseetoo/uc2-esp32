#pragma once
#include "cJSON.h"


#include "../motor/FocusMotor.h"

namespace PidController
{

    /* data */
    long returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd);

    static bool DEBUG = false;

    static float errorRunSum=0;
    static float previousError=0;
    static float stepperMaxValue=2500.;
    static float PID_Kp = 10;
    static float PID_Ki = 0.1;
    static float PID_Kd = 0.1;
    static float PID_target = 500;
    static float PID_updaterate = 200; // ms
    static bool PID_active=false;
    // timing variables
	static unsigned long startMillis;
	static unsigned long currentMillis;

    static int N_analogin_avg = 50;

    void setup();
    void loop();
    int act(cJSON * ob);
    cJSON * get(cJSON * ob);
};
