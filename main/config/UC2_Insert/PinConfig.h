#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct UC2_Insert : PinConfig
{
     /*#define FOCUS_MOTOR
     #define BLUETOOTH
     #define PSXCONTROLLER
     #define LED_CONTROLLER
     #define HOME_MOTOR
     #define LASER_CONTROLLER*/
     const char *pindefName = "UC2UC2_Insert";

     // UC2 STandalone V2
     int8_t MOTOR_A_DIR = disabled;
     int8_t MOTOR_X_DIR = GPIO_NUM_19;
     int8_t MOTOR_Y_DIR = disabled;
     int8_t MOTOR_Z_DIR = disabled;
     int8_t MOTOR_A_STEP = disabled;
     int8_t MOTOR_X_STEP = GPIO_NUM_35;
     int8_t MOTOR_Y_STEP = disabled;
     int8_t MOTOR_Z_STEP = disabled;
     int8_t MOTOR_ENABLE = GPIO_NUM_33;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LASER_1 = disabled;
     int8_t LASER_2 = disabled;
     int8_t LASER_3 = disabled;

     int8_t LED_PIN = GPIO_NUM_4;
     int8_t LED_COUNT = 64;

     int8_t PIN_DEF_END_X = disabled;
     int8_t PIN_DEF_END_Y = disabled;
     int8_t PIN_DEF_END_Z = disabled;

     const char *PSX_MAC = "1a:2b:3c:01:01:01";
     int8_t PSX_CONTROLLER_TYPE = 2;
};
const UC2_Insert pinConfig;