#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct UC2_OMNISCOPE : PinConfig
{
     #define FOCUS_MOTOR
     #define BLUETOOTH
     #define PSXCONTROLLER
     #define WIFI
     #define LED_CONTROLLER
     #define HOME_MOTOR
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

     int8_t LED_PIN = GPIO_NUM_4;
     int8_t LED_COUNT = 128;

     int8_t PIN_DEF_END_X = disabled;
     int8_t PIN_DEF_END_Y = disabled; // GPIO_NUM_5;
     int8_t PIN_DEF_END_Z = disabled; // GPIO_NUM_23;

     const char *PSX_MAC = "1a:2b:3c:01:01:03";
     int8_t PSX_CONTROLLER_TYPE = 2;

     const char *mSSID = "Blynk";   //"omniscope";
     const char *mPWD = "12345678"; //"omniscope";
     bool mAP = false;
};
const UC2_OMNISCOPE pinConfig;