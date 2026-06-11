#pragma once
#include "PinConfig.h"
#include "MotorTypes.h"

namespace MotorGamePad
{

    float getJoystickScaleFactor(); // Get current joystick scaling factor (0.1 fine, 1.0 coarse)
    bool isInFineMode(); // Check if currently in fine mode
    void resetCalibration(); // Reset joystick offset calibration
    static bool axisRunning[MOTOR_AXIS_COUNT] { false };   // assumes Stepper::X..A == 0..3
    void options_changed_event(int pressed);
    void xyza_changed_event(int x, int y, int z, int a);
    void singlestep_event(int left, int right, bool r1, bool r2, bool l1, bool l2);

    // Bridge-only configuration (used when MOTOR_CONTROLLER is not defined,
    // i.e. there is no local FocusMotor::getData() to consult). Defaults to
    // false for every axis. On master / slave builds the value is taken from
    // FocusMotor::getData()[ax]->joystickDirectionInverted instead.
    void setJoystickInverted(int ax, bool inverted);
};
