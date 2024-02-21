#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct XYZ_MOTOR_ENDSTOP_JOYSTICK : PinConfig
{
     /*#define ANALOG_JOYSTICK
     #define FOCUS_MOTOR
     #define BLUETOOTH
     #define BTHID
     #define WIFI*/

     const char *pindefName = "XYZ_MOTOR_ENDSTOP_JOYSTICK";
     const int8_t MOTOR_X_DIR = 16;
     int8_t MOTOR_X_STEP = 26;
     int8_t MOTOR_Y_DIR = 27;
     int8_t MOTOR_Y_STEP = 25;
     int8_t MOTOR_Z_DIR = 14;
     int8_t MOTOR_Z_STEP = 17;
     int8_t MOTOR_ENABLE = 12;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t ANLOG_JOYSTICK_X = 0;
     int8_t ANLOG_JOYSTICK_Y = 0;

     #define HOME_MOTOR
     int8_t PIN_DEF_END_X = 12;
     int8_t PIN_DEF_END_Y = 13;
     int8_t PIN_DEF_END_Z = 5;
};
const XYZ_MOTOR_ENDSTOP_JOYSTICK pinConfig;