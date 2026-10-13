#pragma once
// T2.1: xiao_ledservo app-local PinConfig (converted from
// main/config/seeed_xiao_esp32s3_ledservo/PinConfig.h: Arduino.h -> uc2_hal.h).
// Shadows the shared main/PinConfig.h (laser) for this app's build via -I main/.
// Per-app PinConfig rework lands in T2.11.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// #ifndef guards: the IDF build may already define board/feature flags via
// CMAKE_CXX_FLAGS (-DFOO=1 vs an empty #define FOO would trip -Werror).
#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif
#ifndef LASER_CONTROLLER
#define LASER_CONTROLLER
#endif
#ifndef LED_CONTROLLER
#define LED_CONTROLLER
#endif
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif

struct UC2_ESP32S3_XIAO_LEDSERVO : PinConfig
{
    const char *pindefName = "UC2_esp32s3_xiao_ledservo";
    const unsigned long BAUDRATE = 115200;

    bool ENC_A_encoderDirection = true;
    bool ENC_X_encoderDirection = true;
    bool ENC_Y_encoderDirection = true;
    bool ENC_Z_encoderDirection = true;
    bool ENC_A_motorDirection = true;
    bool ENC_X_motorDirection = true;
    bool ENC_Y_motorDirection = true;
    bool ENC_Z_motorDirection = true;

    bool MOTOR_ENABLE_INVERTED = true;
    bool MOTOR_AUTOENABLE = false;
    int8_t AccelStepperMotorType = 1;

    // Laser control pins
    int8_t LASER_0 = 44;         // D7 (Servo PWM 1)
    int8_t LASER_1 = GPIO_NUM_4; // D3 (Motor 1/2)
    int8_t LASER_2 = GPIO_NUM_3; // D2 (Motor 1/1)
    int8_t LASER_3 = GPIO_NUM_7; // D8 (Servo PWM 2)

    int8_t DIGITAL_OUT_1 = GPIO_NUM_8; // D9 (Motor 2/2)
    int8_t DIGITAL_OUT_2 = GPIO_NUM_9; // D10 (Motor 2/1)
    int8_t DIGITAL_OUT_3 = disabled;

    int8_t LED_PIN = 43;   // D6 Neopixel
    int8_t LED_COUNT = 1;

    int8_t DIGITAL_IN_1 = disabled;
    int8_t DIGITAL_IN_2 = disabled;
    int8_t DIGITAL_IN_3 = disabled;

    int8_t JOYSTICK_SPEED_MULTIPLIER = 30;
    int8_t JOYSTICK_MAX_ILLU = 100;
    int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 30;

    int8_t ENC_X_A = disabled;
    int8_t ENC_Y_A = disabled;
    int8_t ENC_Z_A = disabled;
    int8_t ENC_X_B = disabled;
    int8_t ENC_Y_B = disabled;
    int8_t ENC_Z_B = disabled;

    int8_t I2C_SCL = GPIO_NUM_6; // D5
    int8_t I2C_SDA = GPIO_NUM_5; // D4
};

const UC2_ESP32S3_XIAO_LEDSERVO pinConfig;
