#pragma once
// T3.4: UC2 Standalone on Wemos D1 R32 pin config — converted from the PIO
// main/config/UC2_WEMOS/PinConfig.h (recovered from git history; the PIO copy
// target was retired in T2.11). Arduino.h -> uc2_hal.h. TCA/DAC/DS18B20 pins are
// not wired on this board's minimal header set, so they fall back to the
// PinConfigDefault.h defaults (disabled / GPIO_NUM_NC) even though the feature
// flags are on (env extends UC2_3_default).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

struct UC2_WEMOS : PinConfig // also used for cellSTORM wellplateformat
{
     /*#define MOTOR_CONTROLLER
     #define BLUETOOTH
     #define PSXCONTROLLER
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

     const char *PSX_MAC = "1a:2b:3c:01:01:03";
     int8_t PSX_CONTROLLER_TYPE = 2;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 5;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 3;

         // Additional Digital Input Pins
     int8_t DIGITAL_IN_1 = GPIO_NUM_13;  // Disabled
     int8_t DIGITAL_IN_2 = GPIO_NUM_5;  // VP on ESP32
     int8_t DIGITAL_IN_3 = GPIO_NUM_23;  // VN on ESP32

     const char *mSSID = "Blynk";
     const char *mPWD = "12345678";
     bool mAP = false;
};
const UC2_WEMOS pinConfig;
