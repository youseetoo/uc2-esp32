// T4.1: C API over the FastAccelStepper vendor library (C++ island).
// Implementation in FAccelStep.cpp keeps the vendor C++ types; only this
// header is visible to pure-C modules (FocusMotor.c, axis_motor_c.cpp).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "MotorTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void faccel_step_setup(void);
void faccel_step_setup_axis(Stepper stepper, int motoren, int motordir, int motorstp);
void faccel_step_start(int i);
void faccel_step_stop(int i);
// Wire the vendor engine's GPIO callback to tca_controller::setExternalPin
// (C++ trampoline inside the island; only available with USE_TCA9535).
void faccel_step_set_external_call_for_tca(void);
void faccel_step_update_data(int i);
int32_t faccel_step_get_current_position(Stepper s);
int32_t faccel_step_get_current_position_idx(int i);
void faccel_step_set_auto_enable(bool enable);
void faccel_step_enable(bool enable);
void faccel_step_set_position(Stepper s, int val);
bool faccel_step_is_running(int i);
void faccel_step_move(Stepper s, int steps, bool blocking);

int faccel_step_ramp_state(int i);
int32_t faccel_step_get_current_speed_mhz(int i, bool withSign);
// Live velocity for the axis SERVO loop (mutex-free direct FAS call).
void faccel_step_set_live_speed(int i, int32_t signedSpeed, uint32_t accel);

#ifdef __cplusplus
}
#endif
