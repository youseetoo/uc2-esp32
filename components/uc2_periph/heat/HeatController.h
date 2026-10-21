#pragma once
// HeatController (Phase 4: pure C)
// PID-based heat-sink temperature regulation (drives the laser PWM channel).
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

long   heat_return_control_value(float control_target, float analogin_value, float kp, float ki, float kd);
void   heat_setup(void);
void   heat_loop(void);
int    heat_act(cJSON *ob);
cJSON *heat_get(cJSON *ob);

// Shared state (was namespace-scope statics in the old header; now defined
// once in HeatController.c).
extern bool  heat_active;
extern bool  heat_was_active;
extern float heat_pid_kp;
extern float heat_pid_ki;
extern float heat_pid_kd;
extern float heat_pid_target;
extern float heat_pid_updaterate; // ms
extern int   heat_h_timeout;      // max ms to hold band, otherwise stop
extern int   heat_max_pwm_value;

#ifdef __cplusplus
}  // extern "C"
#endif
