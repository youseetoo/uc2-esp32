// T4.1: pure C (was the C++ MotorGamePad namespace).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "PinConfig.h"
#include "MotorTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

float motor_gamepad_get_joystick_scale_factor(void); // current scaling (0.1 fine, 1.0 coarse)
bool   motor_gamepad_is_in_fine_mode(void);
void   motor_gamepad_reset_calibration(void);
// Array of MOTOR_AXIS_COUNT entries — which axes the gamepad currently drives
// (assumes Stepper::X..A == 0..3).
extern bool motor_gamepad_axis_running[MOTOR_AXIS_COUNT];
void motor_gamepad_options_changed_event(int pressed);
void motor_gamepad_xyza_changed_event(int x, int y, int z, int a);
void motor_gamepad_singlestep_event(int left, int right, bool r1, bool r2, bool l1, bool l2);

// Bridge-only configuration (used when MOTOR_CONTROLLER is not defined,
// i.e. there is no local FocusMotor::getData() to consult). Defaults to
// false for every axis. On master / slave builds the value is taken from
// FocusMotor::getData()[ax]->joystickDirectionInverted instead.
void motor_gamepad_set_joystick_inverted(int ax, bool inverted);

// Bridge-only per-axis joystick speed-scaling multiplier (used when
// MOTOR_CONTROLLER is not defined). Defaults to pinConfig.JOYSTICK_SPEED_MULTIPLIER[_Z].
// On master / slave builds the value is taken from
// FocusMotor::getData()[ax]->joystickSpeedMultiplier instead, which is
// set via /motor_act "speedmult" (see motor_json_parser parseSetJoystickSpeed).
void motor_gamepad_set_joystick_speed_multiplier(int ax, float multiplier);

#ifdef __cplusplus
}
#endif
