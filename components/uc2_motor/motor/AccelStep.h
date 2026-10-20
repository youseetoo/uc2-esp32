// T4.1: C API over the AccelStepper vendor library (C++ island).
// Implementation in AccelStep.cpp keeps the vendor C++ types; only this
// header is visible to pure-C modules.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "MotorTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// FreeRTOS driver tasks (started by the app / FocusMotor paths)
void driveMotorXLoop(void *pvParameter);
void driveMotorYLoop(void *pvParameter);
void driveMotorZLoop(void *pvParameter);
void driveMotorALoop(void *pvParameter);

void accel_step_setup(void);
void accel_step_start(int i);
void accel_step_stop(int i);
void accel_step_set_external_call_for_tca(void);
void accel_step_drive_motor_loop(int stepperid);
void accel_step_update_data(int i);
int32_t accel_step_get_current_position(int i);
void accel_step_enable(bool enable);
void accel_step_set_position(Stepper axis, int pos);
bool accel_step_is_running(int i);
void accel_step_send_motor_pos(int i, int arraypos);

#ifdef __cplusplus
}
#endif
