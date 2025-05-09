#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct HoLiSheet : PinConfig
{
     /*#define MOTOR_CONTROLLER
     #define BLUETOOTH
     #define PSXCONTROLLER
     #define LED_CONTROLLER
     #define HOME_MOTOR*/
     const char *pindefName = "HoLiSheet";
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_17;
     int8_t MOTOR_ENABLE = GPIO_NUM_12;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LASER_1 = GPIO_NUM_18;
     int8_t LASER_2 = GPIO_NUM_19;
     int8_t LASER_3 = disabled; // GPIO_NUM_21

     int8_t LED_PIN = GPIO_NUM_4;
     int8_t LED_COUNT = 64;

     int8_t DIGITAL_IN_1 = GPIO_NUM_13;
     int8_t DIGITAL_IN_2 = GPIO_NUM_5;
     int8_t DIGITAL_IN_3 = GPIO_NUM_23;

     const char *PSX_MAC = "1a:2b:3c:01:01:01";
     int8_t PSX_CONTROLLER_TYPE = 2;

};
const HoLiSheet pinConfig;