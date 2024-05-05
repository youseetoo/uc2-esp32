#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct UC2_1 : PinConfig
{
     /*#define FOCUS_CONTROLLER
     #define BLUETOOTH
     #define PSXCONTROLLER
     #define LED_CONTROLLER
     #define HOME_MOTOR
     #define LASER_CONTROLLER*/
     const char *pindefName = "UC2_1";
     // UC2 STandalone V1
     int8_t MOTOR_A_DIR = GPIO_NUM_21;
     int8_t MOTOR_X_DIR = GPIO_NUM_33;
     int8_t MOTOR_Y_DIR = GPIO_NUM_16;
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;
     int8_t MOTOR_A_STEP = GPIO_NUM_22;
     int8_t MOTOR_X_STEP = GPIO_NUM_2;
     int8_t MOTOR_Y_STEP = GPIO_NUM_27;
     int8_t MOTOR_Z_STEP = GPIO_NUM_12;
     int8_t MOTOR_ENABLE = GPIO_NUM_13;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LASER_1 = GPIO_NUM_4;
     int8_t LASER_2 = GPIO_NUM_15;
     int8_t LASER_3 = disabled; // GPIO_NUM_21

     int8_t LED_PIN = GPIO_NUM_17;
     int8_t LED_COUNT = 25;

     int8_t PIN_DEF_END_X = GPIO_NUM_10;
     int8_t PIN_DEF_END_Y = GPIO_NUM_11;
     int8_t PIN_DEF_END_Z = disabled;

     const char *PSX_MAC = "1a:2b:3c:01:01:05";
     int8_t PSX_CONTROLLER_TYPE = 1;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 20;
};
const UC2_1 pinConfig;