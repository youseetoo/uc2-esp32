#pragma once
#include <stdint.h>

void    stepper_init();
void    stepper_poll();               // call often in loop()
void    stepper_move_to(int32_t pos); // in *step* units
int32_t stepper_get_pos();
int32_t stepper_get_target();
bool    stepper_is_busy();

// Velocity control interface for closed-loop operation
void    stepper_set_velocity(float steps_per_sec);  // set instantaneous velocity
void    stepper_stop();                              // emergency stop
