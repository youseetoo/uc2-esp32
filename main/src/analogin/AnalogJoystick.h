#include <PinConfig.h>"
#ifdef ANALOG_JOYSTICK
#pragma once

#include "../motor/MotorTypes.h"


void processLoopAJoy(void *param);

namespace AnalogJoystick
{

    static int max_in_value = 4096;
    static int zeropoint = 350;
   
    static int joystick_drive_X = 0;
    static int joystick_drive_Y = 0;
    void setup();
    void driveMotor(Stepper s, int joystick_drive,int pin);
};
#endif