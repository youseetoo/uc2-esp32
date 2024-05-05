#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct UC2_2 : PinConfig
{
     /*#define FOCUS_CONTROLLER
     #define BLUETOOTH
     #define PSXCONTROLLER
     #define LED_CONTROLLER
     #define HOME_MOTOR
     #define LASER_CONTROLLER
     #define DIGITAL_IN_CONTROLLER*/
     const char *pindefName = "UC2_2";

     // UC2 STandalone V2
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
     bool MOTOR_AUTOENABLE = true;

     int8_t LASER_1 = GPIO_NUM_17;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_15;

     int8_t LED_PIN = GPIO_NUM_32;
     int8_t LED_COUNT = 64;

     // FIXME: Is this redudant?!
     int8_t PIN_DEF_END_X = GPIO_NUM_34;
     int8_t PIN_DEF_END_Y = GPIO_NUM_39;
     int8_t PIN_DEF_END_Z = disabled;
     int8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     int8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     int8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     const char *PSX_MAC = "1a:2b:3c:01:01:04";
     int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4

     int8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 30;
};
const UC2_2 pinConfig;