//This is for the standalone UC2 v4 board in CAN HYBRID MODE
/*
HYBRID MODE: This configuration enables the UC2 v4 board to operate with:
- Native on-board motor drivers (FastAccelStepper) for axes 0-3 (A, X, Y, Z)
- CAN-connected satellite boards for axes 4+ (B, C, D, E...)
- Native laser drivers for lasers 0-3
- CAN-connected lasers for laser 4+
- Optional dual output for LED arrays (both local and CAN)

DIP Switch Configuration:
- DIP1-4: Reserved for future use
- See hardware documentation for CAN termination settings
*/

#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

// Enable hybrid CAN master mode with native motor drivers
#define MOTOR_CONTROLLER
#define CAN_CONTROLLER
#define CAN_MASTER
#define USE_FASTACCEL
#define USE_TCA9535
#define BLUETOOTH
#define BTHID
//#define WIFI
#define HOME_MOTOR
#define LASER_CONTROLLER
#define DIGITAL_IN_CONTROLLER 
#define LED_CONTROLLER

// Extend motor count to include CAN satellites
#undef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 10

struct UC2_4_CAN_HYBRID : PinConfig
{
     /*
     UC2 v4 Standalone board in HYBRID mode:
     - Native motors A, X, Y, Z (axes 0-3)
     - CAN bus motors B, C, D, E, F, G (axes 4-9)
     - Native lasers 0-3
     - CAN bus lasers 4+
     */
  
     const char * pindefName = "UC2_4_CAN_HYBRID";
     const unsigned long BAUDRATE = 115200;
     bool DEBUG_CAN_ISO_TP = 0;

     // Native motor drivers (on-board via TCA9535)
     int8_t MOTOR_A_STEP = GPIO_NUM_15;
     int8_t MOTOR_X_STEP = GPIO_NUM_16;
     int8_t MOTOR_Y_STEP = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;
     bool isDualAxisZ = false;

     // Direction pins live on TCA9535
     int8_t MOTOR_ENABLE = 0; 
     int8_t MOTOR_X_DIR = 1;
     int8_t MOTOR_Y_DIR = 2;
     int8_t MOTOR_Z_DIR = 3;
     int8_t MOTOR_A_DIR = 4;

     // CAN motor axes (B-G) - no native step pins, use CAN
     // MOTOR_B_STEP through MOTOR_G_STEP remain disabled (-1) from default
     // They will be routed to CAN satellites based on HYBRID_MOTOR_CAN_THRESHOLD

     const bool dumpHeap = false;
     const uint16_t DEFAULT_TASK_PRIORITY = 0;
     const uint16_t MAIN_TASK_STACKSIZE = 8128;
     const uint16_t ANALOGJOYSTICK_TASK_STACKSIZE = 0;
     const uint16_t HIDCONTROLLER_EVENT_STACK_SIZE = 4* 2048;
     const uint16_t HTTP_MAX_URI_HANDLERS = 35;
     const uint16_t BT_CONTROLLER_TASK_STACKSIZE = 4 * 2048;
     const uint16_t MOTOR_TASK_STACKSIZE = 4 * 1024;
     const uint16_t MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE = 0;
     const uint16_t INTERRUPT_CONTROLLER_TASK_STACKSIZE = 0;
     const uint16_t TCA_TASK_STACKSIZE = 1024;
     const uint16_t SCANNER_TASK_STACKSIZE = 0;
     const uint16_t TEMPERATURE_TASK_STACKSIZE = 0;

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     int8_t AccelStepperMotorType = 1;

     // Native laser drivers (PWM channels 1-4)
     int8_t LASER_1 = GPIO_NUM_12;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_2;
     int8_t LASER_4 = GPIO_NUM_17;

     // Native LED array
     int8_t LED_PIN = GPIO_NUM_13;
     int8_t LED_COUNT = 64;

     // Digital inputs on TCA9535
     int8_t DIGITAL_IN_1 = 5;
     int8_t DIGITAL_IN_2 = 6;
     int8_t DIGITAL_IN_3 = 7;
     
     int8_t dac_fake_1 = disabled;
     int8_t dac_fake_2 = disabled;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 2;
     int8_t JOYSTICK_MAX_ILLU = 255;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 1;
     
     // I2C for TCA9535 port expander
     int8_t I2C_SCL = GPIO_NUM_22;
     int8_t I2C_SDA = GPIO_NUM_21;
     int8_t I2C_ADD_TCA = 0x27;
     gpio_num_t I2C_INT = GPIO_NUM_27;

     int8_t I2C_ADD_MOT_X = 0x40;
     int8_t I2C_ADD_MOT_Y = 0x41;
     int8_t I2C_ADD_MOT_Z = 0x42;
     int8_t I2C_ADD_MOT_A = 0x43;
     int8_t I2C_ADD_LEX_MAT = 0x50;
     int8_t I2C_ADD_LEX_PWM1 = 0x51;
     int8_t I2C_ADD_LEX_PWM2 = 0x52;
     int8_t I2C_ADD_LEX_PWM3 = 0x53;
     int8_t I2C_ADD_LEX_PWM4 = 0x54;

     // SPI (unused but configured)
     int8_t SPI_MOSI = GPIO_NUM_23;
     int8_t SPI_MISO = GPIO_NUM_19;
     int8_t SPI_SCK = GPIO_NUM_18;
     int8_t SPI_CS = GPIO_NUM_5;

     // CAN bus configuration
     int8_t CAN_RX = GPIO_NUM_33;
     int8_t CAN_TX = GPIO_NUM_32;
     uint32_t CAN_ID_CURRENT = CAN_ID_CENTRAL_NODE;

     // ========================================================================
     // HYBRID MODE CONFIGURATION
     // ========================================================================
     // Motors: axes 0-3 use native FastAccelStepper, axes 4+ use CAN
     // Lasers: lasers 0-3 use native PWM, laser 4+ uses CAN
     // LEDs: can output to both native LED array AND CAN LED devices
     // ========================================================================
     
     // Axis threshold for CAN routing (default: 4 means A,X,Y,Z are native; B,C,D,E,F,G use CAN)
     uint8_t HYBRID_MOTOR_CAN_THRESHOLD = 4;
     
     // Laser threshold for CAN routing (default: 4 means lasers 0-3 are native)
     uint8_t HYBRID_LASER_CAN_THRESHOLD = 4;
     
     // Set to true to send LED commands to BOTH native array AND CAN devices
     bool HYBRID_LED_DUAL_OUTPUT = false;
};
const UC2_4_CAN_HYBRID pinConfig;
