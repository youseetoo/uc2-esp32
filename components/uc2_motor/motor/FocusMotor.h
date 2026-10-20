// T4.1: pure C (was a C++ namespace).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cJsonTool.h"
#include "uc2_hal.h"
#include "JsonKeys.h"
#include "ConfigController.h"
#include "uc2_hal/uc2_prefs.h"
#ifdef USE_TCA9535
#include "tca9535.h"
#endif
#include "MotorTypes.h"

#ifdef MOTOR_CONTROLLER
#if !defined USE_FASTACCEL && !defined USE_ACCELSTEP && !defined DIAL_CONTROLLER && !defined CAN_CONTROLLER_CANOPEN
#error Pls set USE_FASTACCEL or USE_ACCELSTEP
#endif
#endif
#if defined USE_FASTACCEL && defined USE_ACCELSTEP
#error Pls set only USE_FASTACCEL or USE_ACCELSTEP, currently both are active
#endif

#ifdef __cplusplus
extern "C" {
#endif

void focusmotor_setup(void);
void focusmotor_loop(void);
void focusmotor_fill_data(void); // populate data[] from pinConfig + NVS
// Array of MOTOR_AXIS_COUNT entries tracking which motors are activated.
extern bool focusmotor_isActivated[MOTOR_AXIS_COUNT];

void focusmotor_stop_stepper(int i);
void focusmotor_start_stepper(int i, int reduced);
void focusmotor_send_motor_pos(int i, int arraypos, int qid);
void focusmotor_set_position(Stepper s, int pos);
void focusmotor_move(Stepper s, int steps, bool blocking);
bool focusmotor_is_running(int i);
// reduced => 0: full values MotorData, 1: reduced MotorDataReduced, 2: single value for CAN
void focusmotor_toggle_stepper(Stepper s, bool isStop, int reduced);
void focusmotor_set_auto_enable(bool enable);
void focusmotor_set_enable(bool enable);
void focusmotor_update_data(int axis); // pull motor data to the data-array
MotorData **focusmotor_get_data(void);
void focusmotor_set_data(int axis, MotorData *data);
void focusmotor_move_motor(int32_t pos, int32_t speed, int s, bool isRelative);

// Motor position functions
int32_t focusmotor_get_current_motor_position(int axis); // real-time position from the stepper backend
uint32_t focusmotor_get_position(Stepper s); // updateData + return currentPosition

// Hard limit functions (emergency stop on endstop hit during normal operation)
// Only relevant on CAN slave/satellite motor controllers
void focusmotor_set_hard_limit(int axis, bool enabled, bool polarity);
void focusmotor_check_hard_limits(void); // called in loop; emergency stop if triggered (slaves only)
void focusmotor_clear_hard_limit_triggered(int axis); // after successful homing
// Set + persist (NVS) the directional hard-limit lockout so a stage trapped
// against an endstop keeps the correct escape direction across a reboot.
void focusmotor_set_hard_limit_lockout_dir(int axis, int8_t dir);

#ifdef __cplusplus
}
#endif
