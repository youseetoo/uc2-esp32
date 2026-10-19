#pragma once
// T4.1: PidController converted from a C++ namespace to a pure-C module
// (see QidRegistry.h for the pattern). The C++ FocusMotor dependency
// (stop/start the focus stepper, gated on MOTOR_CONTROLLER) is hidden behind
// the focusmotor_qid_c.h C shim (same component, uc2_motor).
#include <stdbool.h>
#include <stdint.h>
#include <cJSON.h>
#include "PinConfigDefault.h"  // pinConfig.pid1/pid2/pid3 (pure C; PinConfig.h is C++)

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the PID controller (records the start timestamp).
void pid_setup(void);

// Periodic PID tick — call from the app loop (was PidController::loop()).
void pid_loop(void);

// Process /pid_act JSON command. Returns the qid.
int pid_act(cJSON *ob);

// Process /pid_get JSON command. Returns {"pid":{...}} (caller owns the
// returned cJSON object).
cJSON *pid_get(cJSON *ob);

// PID core math (was PidController::returnControlValue):
// returns the stepper output clamped to ±2500 (updated internal I/D state).
int32_t pid_return_control_value(float controlTarget, float analoginValue,
                                 float Kp, float Ki, float Kd);

#ifdef __cplusplus
}
#endif
