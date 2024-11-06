#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
struct UC2e : PinConfig
{
     /*#define MOTOR_CONTROLLER
     #define LED_CONTROLLER
     #define HOME_MOTOR
     #define LASER_CONTROLLER
     #define DAC_CONTROLLER
     #define ANALOG_IN_CONTROLLER
     #define DIGITAL_IN_CONTROLLER*/
     const char *pindefName = "UC2e";

     // UC2e (UC2-express-bus) pinout of the SUES system (modules and baseplane) dictated by the spec for the ESP32 module pinout
     // that document maps ESP32 pins to UC2e pins (and pin locations on the physical PCIe x1 connector
     // and assigns functions compatible with the ESP32 GPIO's hardware capabilities.
     // See https://github.com/openUC2/UC2-SUES/blob/main/uc2-express-bus.md#esp32-module-pinout

     // Stepper Motor control pins
     int8_t MOTOR_X_DIR = GPIO_NUM_12;  // STEP-1_DIR, PP_05, PCIe A8
     int8_t MOTOR_Y_DIR = GPIO_NUM_13;  // STEP-2_DIR, PP_06, PCIe A9
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;  // STEP-3_DIR, PP_07, PCIe A10
     int8_t MOTOR_A_DIR = GPIO_NUM_15;  // STEP-4_DIR, PP_08, PCIe A11
     int8_t MOTOR_X_STEP = GPIO_NUM_0;  // STEP-1_STP, PP_01, PCIe A1
     int8_t MOTOR_Y_STEP = GPIO_NUM_2;  // STEP-2_STP, PP_02, PCIe A5
     int8_t MOTOR_Z_STEP = GPIO_NUM_4;  // STEP-3_STP, PP_03, PCIe A6
     int8_t MOTOR_A_STEP = GPIO_NUM_5;  // STEP-4_STP, PP_04, PCIe A7
     int8_t MOTOR_ENABLE = GPIO_NUM_17; // STEP_EN, PP_09, PCIe A13. Enables/disables all stepper drivers' output.
     bool MOTOR_ENABLE_INVERTED = true; // When set to a logic high, the outputs are disabled.

     // LED pins (addressable - WS2812 and equivalent)
     int8_t NEOPIXEL_PIN = GPIO_NUM_18; // NEOPIXEL, PP_10, PCIe A14. I changed the name from LED_PIN to indicate that it's for an adressable LED chain
     int8_t LED_COUNT = 64;             // I havent touched this

     // Main power switch for all the AUX modules of the baseplane
     int8_t BASEPLANE_ENABLE = GPIO_NUM_16; // UC2e_EN, ENABLE, PCIe A17. ESP32 module in Controller slot of baseplane switches power off all AUX modules. HIGH activates AUX modules.

     // Pulse Width Modulation pins
     int8_t PWM_1 = GPIO_NUM_19; // PWM_1, PP_11, PCIe A16
     int8_t PWM_2 = GPIO_NUM_23; // PWM_2, PP_12, PCIe B8
     int8_t PWM_3 = GPIO_NUM_27; // PWM_3, PP_15, PCIe B11
     int8_t PWM_4 = GPIO_NUM_32; // PWM_4, PP_16, PCIe B12

     // Digital-to-Analog converter pins
     int8_t DAC_1 = GPIO_NUM_25; // DAC_1, PP_13, PCIe B9
     int8_t DAC_2 = GPIO_NUM_27; // DAC_2, PP_14, PCIe B10

     // Other pins that sense PCIe/housekeeping things
     int8_t UC2E_OWN_ADDR = GPIO_NUM_33; // Analog input! The voltage indicates into which slot of the baseplane the ESP32 module is plugged.
     int8_t UC2E_VOLTAGE = GPIO_NUM_34;  // Analog input! Can measure the (12V) baseplane power via a voltage divider.

     // Sensor pins, Analog-to-digital converter enabled
     int8_t SENSOR_1 = GPIO_NUM_35; // SENSOR_1, PP_17, PCIe B14
     int8_t SENSOR_2 = GPIO_NUM_36; // SENSOR_2, PP_18, PCIe B15
     int8_t SENSOR_3 = GPIO_NUM_39; // SENSOR_3, PP_19, PCIe B17

     // I2C pins, digital bus to control various functions of the baseplane and SUES modules
     int8_t I2C_SDA = GPIO_NUM_21; // I2C_SDA, SDA, PCIe B6
     int8_t I2C_SCL = GPIO_NUM_22; // I2C_SCL, SCL, PCIe B5
};
const UC2e pinConfig;