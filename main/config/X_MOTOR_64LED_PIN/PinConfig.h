#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct X_MOTOR_64LED_PIN : PinConfig
{
     #define FOCUS_MOTOR
     #define LED_CONTROLLER
     const char *pindefName = "X_Motor_64LED";
     int8_t MOTOR_X_DIR = 21;
     int8_t MOTOR_X_STEP = 19;
     int8_t MOTOR_ENABLE = 18;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LED_PIN = 27;
     int8_t LED_COUNT = 64;
};
const X_MOTOR_64LED_PIN pinConfig;