#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#include "M5Dial.h"
#undef PSXCONTROLLER

// ATTENTION: THIS IS FOR LINTING AND DEFINES!
#define CORE_DEBUG_LEVEL
#define M5DIAL
#define DIAL_CONTROLLER
#define CAN_SEND_COMMANDS
#define CAN_BUS_ENABLED
#define LASER_CONTROLLER

// Explicitly disable all unnecessary controllers
#undef MOTOR_CONTROLLER
#undef HOME_MOTOR
#undef BLUETOOTH
#undef BTHID
#undef TMC_CONTROLLER
#undef OBJECTIVE_CONTROLLER
#undef STAGE_SCAN
#undef LED_CONTROLLER
#undef GALVO_CONTROLLER
#undef DAC_CONTROLLER
#undef DIGITAL_IN_CONTROLLER
#undef MESSAGE_CONTROLLER
#undef ANALOG_IN_CONTROLLER
#undef ANALOG_OUT_CONTROLLER
#undef HEAT_CONTROLLER
#undef PID_CONTROLLER
#undef SCANNER_CONTROLLER
#undef I2C_MASTER
#undef I2C_SLAVE_MOTOR
#undef I2C_SLAVE_LASER
#undef I2C_SLAVE_DIAL
#undef LINEAR_ENCODER_CONTROLLER

struct UC2_M5StackDial : PinConfig
{
     /*
     This is the M5Stack Dial Pin Configuration 
     Configured as CAN Master for direct motor/laser control
     
     M5Dial (ESP32-S3) GPIO Pinout:
     - Built-in: Display, Encoder (G40/G41), Speaker (G14), Touch screen
     - Internal I2C: G13 (SDA), G15 (SCL)
     - Grove Port: G1 (Yellow/TX), G2 (White/RX)
     - Battery management and charging circuit
     
     For CAN bus, use the Grove connector with external CAN transceiver:
     - CAN TX: G1 (Grove Yellow wire - requires SN65HVD230 or similar)
     - CAN RX: G2 (Grove White wire)
     */

     const char * pindefName = "UC2_M5StackDial_CAN";
     const unsigned long BAUDRATE = 115200;

     // CAN Bus Pins (via Grove PORT.B - requires CAN transceiver like SN65HVD230)
     // NOTE: Cannot use PORT.A (G13/G15) as M5Dial reserves those for internal I2C
     int8_t CAN_TX = GPIO_NUM_2;   // Grove PORT.B Yellow wire - TWAI TX
     int8_t CAN_RX = GPIO_NUM_1;   // Grove PORT.B White wire - TWAI RX
     
     // CAN Configuration
     uint8_t CAN_ID_CENTRAL_NODE = 1;  // This dial acts as central node/master
     
     // Motor CAN IDs (matching default slave configuration)
     uint8_t CAN_ID_MOT_A = 10;  // Axis A motor
     uint8_t CAN_ID_MOT_X = 11;  // Axis X motor
     uint8_t CAN_ID_MOT_Y = 12;  // Axis Y motor
     uint8_t CAN_ID_MOT_Z = 13;  // Axis Z motor
     uint8_t CAN_ID_MOT_B = 14;
     uint8_t CAN_ID_MOT_C = 15;
     uint8_t CAN_ID_MOT_D = 16;
     uint8_t CAN_ID_MOT_E = 17;
     uint8_t CAN_ID_MOT_F = 18;
     uint8_t CAN_ID_MOT_G = 19;
     
     // Laser CAN IDs
     uint8_t CAN_ID_LASER_0 = 20;  // Primary laser/illumination
     uint8_t CAN_ID_LASER_1 = 21;
     uint8_t CAN_ID_LASER_2 = 22;
     uint8_t CAN_ID_LASER_3 = 23;
     uint8_t CAN_ID_LASER_4 = 24;
     
     // Disable I2C (not used in CAN mode)
     int8_t I2C_SDA = -1; 
     int8_t I2C_SCL = -1; 
     
     // Disable all motor pins (motors are controlled via CAN)
     int8_t MOTOR_A_STEP = -1;
     int8_t MOTOR_X_STEP = -1;
     int8_t MOTOR_Y_STEP = -1;
     int8_t MOTOR_Z_STEP = -1;
     
     // Disable laser pins (lasers are controlled via CAN)
     int8_t LASER_1 = -1;
     int8_t LASER_2 = -1;
     int8_t LASER_3 = -1;
     
     // LED configuration (if needed for status indication)
     int8_t LED_PIN = -1;
     int8_t LED_COUNT = 0;
     
     // Debug settings
     bool DEBUG_CAN_ISO_TP = true;
     uint32_t CAN_ID_CURRENT = 100; // Default temporary ID before set by master

     // WiFi (optional, can be enabled for OTA updates)
     const char *mSSID = "UC2-M5Dial";
     const char *mPWD = "";
     bool mAP = false;
     const char *mSSIDAP = "UC2-M5Dial";
     const char *hostname = "uc2-dial";

};
const UC2_M5StackDial pinConfig;