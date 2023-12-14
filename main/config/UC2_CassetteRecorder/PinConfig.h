#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct UC2_CassetteRecorder : PinConfig
{
     #define FOCUS_MOTOR

     const char *pindefName = "CassetteRecorder";
     bool useFastAccelStepper = false;
     int8_t AccelStepperMotorType = 8;
     // ESP32-WEMOS D1 R32
     int8_t MOTOR_X_STEP = GPIO_NUM_13;
     int8_t MOTOR_X_DIR = GPIO_NUM_14;
     int8_t MOTOR_X_0 = GPIO_NUM_12;
     int8_t MOTOR_X_1 = GPIO_NUM_27;
     bool ROTATOR_ENABLE = true;
};
const UC2_CassetteRecorder pinConfig;