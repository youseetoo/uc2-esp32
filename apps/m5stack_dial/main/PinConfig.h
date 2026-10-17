#pragma once
// T5.4: M5Stack Dial pin config — converted from the PIO main/config/m5stack_dial/
// PinConfig.h (recovered from git history; the PIO copy target was retired in
// T2.11). Arduino.h -> uc2_hal.h; feature #defines wrapped in #ifndef guards
// because the app's CMakeLists already passes them as -D (a bare #define would
// trip -Werror on redefinition). The M5Dial.h include is dropped here: it would
// pull the whole M5GFX/M5Unified header tree into every component that includes
// PinConfig.h (they include M5Dial.h directly where needed). The M5DIAL app
// define (not a header) is what gates the dial display path.
//
// CANopen ORIGINATOR node (NODE_ROLE=2): writes the same expedited SDOs the
// master would, so motors/lasers work with no master on the bus. No actuator
// controllers are compiled in (same pattern as UC2_canopen_bridge_ptz / _ps4_usbhost).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef M5DIAL
#define M5DIAL 1
#endif
#ifndef DIAL_CONTROLLER
#define DIAL_CONTROLLER 1
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED 1
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN 1
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER 1
#endif
// Board-specific (not in the CXX_FLAGS -D set) — direct.
#define CAN_SEND_COMMANDS

// Explicitly disable all unnecessary controllers (dial-only board).
#undef LASER_CONTROLLER
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
#undef ANALOG_IN_CONTROLLER
#undef ANALOG_OUT_CONTROLLER
#undef HEAT_CONTROLLER
#undef PID_CONTROLLER
#undef SCANNER_CONTROLLER
#undef I2C_MASTER
#undef I2C_SLAVE_DIAL
#undef LINEAR_ENCODER_CONTROLLER

struct UC2_M5StackDial : PinConfig
{
     /*
     This is the M5Stack Dial Pin Configuration
     Configured as CANopen originator node for direct motor/laser control

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

     // CAN Bus Pins (via Grove connector - requires CAN transceiver like SN65HVD230)
     int8_t CAN_TX = 2;//13;   // Grove Yellow wire - TWAI TX
     int8_t CAN_RX = 1;//15;   // Grove White wire - TWAI RX

     // Own CANopen node-id: 62, right after the PTZ bridge (61) and the GPIO
     // slave (60), outside the motor (10..19) / laser (20..) / LED (30) ranges.
     uint32_t CAN_ID_CURRENT = 62;

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

     // Laser channels 0..3 all live on the illumination node 0x14, OD sub 1..4
     // (same map as UC2_canopen_master so the dial addresses the same hardware).
     uint8_t CAN_NODE_LASER[4]    = {0x14, 0x14, 0x14, 0x14};
     int8_t  CAN_SUBAXIS_LASER[4] = {0, 1, 2, 3};

     // Routing — everything the dial touches is REMOTE (1); the rest OFF (2).
     // Indexed by Stepper enum: A=0, X=1, Y=2, Z=3.
     int8_t ROUTE_MOTOR[4] = {1, 1, 1, 1};
     int8_t ROUTE_HOME[4]  = {2, 2, 2, 2};
     int8_t ROUTE_TMC[4]   = {2, 2, 2, 2};
     int8_t ROUTE_LASER[4] = {1, 1, 1, 1};
     int8_t ROUTE_LED      = 1;   // LED matrix on CAN_ID_LED_0 (default 30)

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
     bool DEBUG_CAN_ISO_TP = false;

     // WiFi (optional, can be enabled for OTA updates)
     const char *mSSID = "UC2-M5Dial";
     const char *mPWD = "";
     bool mAP = false;
     const char *mSSIDAP = "UC2-M5Dial";
     const char *hostname = "uc2-dial";

};
const UC2_M5StackDial pinConfig;
