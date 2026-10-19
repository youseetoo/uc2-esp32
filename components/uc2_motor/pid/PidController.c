// T4.1: PidController — pure-C PID controller (was a C++ namespace).
// millis() -> esp_timer; analogRead -> uc2_adc (C API); log_d -> ESP_LOGD;
// String-concat debug line -> one printf-style ESP_LOGD. FocusMotor C++ calls
// (gated on MOTOR_CONTROLLER) go through the focusmotor_qid_c.h C shim.
#include "PidController.h"

#include "cJsonTool.h"    // cjson_tool_* C helpers
#include "uc2_hal/uc2_adc.h"  // analogRead
#include "esp_log.h"
#include "esp_timer.h"
#include "JsonKeys.h"

#ifdef MOTOR_CONTROLLER
#include "focusmotor_qid_c.h"
#endif

static const char *TAG = "PidController";

// ── Tunables + state (was PidController:: static namespace members) ────────
static bool isDEBUG = false;
static float errorRunSum = 0;
static float previousError = 0;
static float stepperMaxValue = 2500.;
static float PID_Kp = 10;
static float PID_Ki = 0.1;
static float PID_Kd = 0.1;
static float PID_target = 500;
static float PID_updaterate = 200; // ms
static bool PID_active = false;
// timing variables
static uint32_t startMillis;
static uint32_t currentMillis;
static int N_analogin_avg = 50;

// Custom function accessible by the API
int pid_act(cJSON *ob)
{
    int qid = cjson_tool_get_int(ob, "qid", 0);
    PID_active = cjson_tool_get_int(ob, key_PIDactive, 0);
    // Behavior preserved: the C++ 2-arg getJsonInt defaults to 0 on missing key.
    PID_Kp = (float)cjson_tool_get_int(ob, key_Kp, 0);
    PID_Ki = (float)cjson_tool_get_int(ob, key_Ki, 0);
    PID_Kd = (float)cjson_tool_get_int(ob, key_Kd, 0);
    PID_target = (float)cjson_tool_get_int(ob, key_target, 0);
    PID_updaterate = (float)cjson_tool_get_int(ob, key_PID_updaterate, 0);
    // here you can do something
    if (isDEBUG)
        ESP_LOGD(TAG, "PID_act_fct");

    if (!PID_active)
    {
// force shutdown the motor
#ifdef MOTOR_CONTROLLER
        focusmotor_set_speed(1, 0);
        focusmotor_stop_stepper(1);
#endif
    }
    return qid;
}

void pid_loop(void)
{
    currentMillis = (uint32_t)(esp_timer_get_time() / 1000);
    if (PID_active && (currentMillis - startMillis >= (uint32_t)PID_updaterate))
    {
        // hardcoded for now:
        int analoginpin = pinConfig.pid1;

        // get rid of noise?
        float analoginValueAvg = 0;
        for (int imeas = 0; imeas < N_analogin_avg; imeas++)
        {
            analoginValueAvg += (float)analogRead(analoginpin);
        }

        analoginValueAvg = analoginValueAvg / (float)N_analogin_avg;
        int32_t motorValue = pid_return_control_value(PID_target, analoginValueAvg, PID_Kp, PID_Ki, PID_Kd);
#ifdef MOTOR_CONTROLLER
        focusmotor_set_isforever(1, 1); // run motor at certain speed
        focusmotor_set_speed(1, motorValue);
        focusmotor_start_stepper(1, 0);
#endif
        startMillis = (uint32_t)(esp_timer_get_time() / 1000);
    }
}

int32_t pid_return_control_value(float controlTarget, float analoginValue, float Kp, float Ki, float Kd)
{
    float analoginOffset = 0.;
    float maxError = 1.;
    float error = (controlTarget - (analoginValue - analoginOffset)) / maxError;
    float cP = Kp * error;
    float cI = Ki * errorRunSum;
    float cD = Kd * (error - previousError);
    float PID = cP + cI + cD;
    int32_t stepperOut = (int32_t)PID;

    if (stepperOut > (int32_t)stepperMaxValue)
    {
        stepperOut = (int32_t)stepperMaxValue;
    }

    if (stepperOut < -(int32_t)stepperMaxValue)
    {
        stepperOut = -(int32_t)stepperMaxValue;
    }

    errorRunSum = errorRunSum + error;
    previousError = error;

    if (isDEBUG)
        // One printf-style line (the C++ version built a String and logged it).
        ESP_LOGD(TAG, "analoginValue: %f, P: %f, I: %f, D: %f, errorRunSum: %f, previousError: %f, stepperOut: %d",
                 (double)analoginValue, (double)cP, (double)cI, (double)cD,
                 (double)errorRunSum, (double)previousError, (int)stepperOut);
    return stepperOut;
}

// Custom function accessible by the API
// returns json {"pid":{...}} as qid
cJSON *pid_get(cJSON *ob)
{
    if (isDEBUG)
        ESP_LOGD(TAG, "PID_get_fct");
    int PIDID = cjson_tool_get_int(ob, key_PIDID, 0);
    int PIDPIN = 0;
    switch (PIDID)
    {
    case 0:
        PIDPIN = pinConfig.pid1;
        break;
    case 1:
        PIDPIN = pinConfig.pid2;
        break;
    case 2:
        PIDPIN = pinConfig.pid3;
        break;
    }

    cJSON *ret = cJSON_CreateObject();
    cJSON *pd = cJSON_CreateObject();
    cJSON_AddItemToObject(ret, key_pid, pd);
    cjson_tool_set_int(pd, key_N_analogin_avg, N_analogin_avg);
    cjson_tool_set_int(pd, key_PIDPIN, PIDPIN);
    cjson_tool_set_int(pd, key_PIDID, PIDID);
    return ret;
}

void pid_setup(void)
{
    ESP_LOGD(TAG, "Setup PID");
    startMillis = (uint32_t)(esp_timer_get_time() / 1000);
}
