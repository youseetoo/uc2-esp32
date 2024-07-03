#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct UC2_XYZRotator : PinConfig
{
     //#define MOTOR_CONTROLLER

     const char *pindefName = "UC2_XYZRotator";

     int8_t AccelStepperMotorType = 8;
     // ESP32-WEMOS D1 R32
     int8_t MOTOR_X_STEP = GPIO_NUM_13;
     int8_t MOTOR_X_DIR = GPIO_NUM_14;
     int8_t MOTOR_X_0 = GPIO_NUM_12;
     int8_t MOTOR_X_1 = GPIO_NUM_27;
     int8_t MOTOR_Y_STEP = GPIO_NUM_26;
     int8_t MOTOR_Y_DIR = GPIO_NUM_33;
     int8_t MOTOR_Y_0 = GPIO_NUM_25;
     int8_t MOTOR_Y_1 = GPIO_NUM_32;
     int8_t MOTOR_Z_STEP = GPIO_NUM_5;
     int8_t MOTOR_Z_DIR = GPIO_NUM_19;
     int8_t MOTOR_Z_0 = GPIO_NUM_18;
     int8_t MOTOR_Z_1 = GPIO_NUM_21;

};
const UC2_XYZRotator pinConfig;