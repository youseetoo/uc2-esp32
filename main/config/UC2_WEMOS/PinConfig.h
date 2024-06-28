#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct UC2_WEMOS : PinConfig // also used for cellSTORM wellplateformat
{
     /*#define MOTOR_CONTROLLER
     #define BLUETOOTH
     #define PSXCONTROLLER
     #define WIFI
     #define LED_CONTROLLER
     #define HOME_MOTOR
     #define LASER_CONTROLLER*/
     const char *pindefName = "UC2_WEMOS";
     // ESP32-WEMOS D1 R32
     int8_t MOTOR_A_DIR = GPIO_NUM_23; // Bridge from Endstop Z to Motor A (GPIO_NUM_23)
     int8_t MOTOR_X_DIR = GPIO_NUM_16;
     int8_t MOTOR_Y_DIR = GPIO_NUM_27;
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;
     int8_t MOTOR_A_STEP = GPIO_NUM_5; // Bridge from Endstop Y to Motor A (GPIO_NUM_5)
     int8_t MOTOR_X_STEP = GPIO_NUM_26;
     int8_t MOTOR_Y_STEP = GPIO_NUM_25;
     int8_t MOTOR_Z_STEP = GPIO_NUM_17;
     int8_t MOTOR_ENABLE = GPIO_NUM_12;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LASER_1 = GPIO_NUM_18; // WEMOS_D1_R32_SPINDLE_ENABLE_PIN
     int8_t LASER_2 = GPIO_NUM_19; // WEMOS_D1_R32_SPINDLEPWMPIN
     int8_t LASER_3 = GPIO_NUM_13; // WEMOS_D1_R32_X_LIMIT_PIN

     int8_t LED_PIN = GPIO_NUM_4;
     int8_t LED_COUNT = 64;

     int8_t PIN_DEF_END_X = 0;
     int8_t PIN_DEF_END_Y = disabled; // GPIO_NUM_5;
     int8_t PIN_DEF_END_Z = disabled; // GPIO_NUM_23;

     const char *PSX_MAC = "1a:2b:3c:01:01:03";
     int8_t PSX_CONTROLLER_TYPE = 2;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 5;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 3;

     const char *mSSID = "Blynk";
     const char *mPWD = "12345678";
     bool mAP = false;
};
const UC2_WEMOS pinConfig;