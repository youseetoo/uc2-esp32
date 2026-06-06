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

// Enable hybrid CAN mode with native motor drivers
#define MOTOR_CONTROLLER
#define CAN_BUS_ENABLED          // Enable CAN hardware
#define CAN_SEND_COMMANDS        // This device sends commands to CAN slaves
#define CAN_CONTROLLER_CANOPEN   // Use the CANopen stack (DeviceRouter REMOTE path)
#define USE_FASTACCEL
#define USE_TCA9535
//#define BLUETOOTH
//#define BTHID
#define CAN_HYBRID
//#define WIFI
#define HOME_MOTOR
#define LASER_CONTROLLER
#define DIGITAL_IN_CONTROLLER 
#define LED_CONTROLLER
#define UC2_FORCE_BT_CLASSIC_ONLY=1
#define BTHID=1
#define BLUETOOTH=1

// Extend motor count to include CAN satellites
#undef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 4

struct UC2_canopen_standalone_v4 : PinConfig
{
     /*
     UC2 v4 Standalone board in HYBRID mode:
     - Native motors A, X, Y, Z (axes 0-3)
     - CAN bus motors B, C, D, E, F, G (axes 4-9)
     - Native lasers 0-3
     - CAN bus lasers 4+
     */
  
     const char * pindefName = "UC2_canopen_standalone_v4";
     const unsigned long BAUDRATE = 115200;
     bool DEBUG_CAN_ISO_TP = 1;

     // Native motor drivers (on-board via TCA9535)
     int8_t MOTOR_A_STEP = GPIO_NUM_15;
     int8_t MOTOR_X_STEP = GPIO_NUM_16;
     int8_t MOTOR_Y_STEP = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;

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
     const uint16_t HIDCONTROLLER_EVENT_STACK_SIZE = 2 * 2048;
     const uint16_t HTTP_MAX_URI_HANDLERS = 35;
     const uint16_t BT_CONTROLLER_TASK_STACKSIZE = 2 * 2048;
     const uint16_t MOTOR_TASK_STACKSIZE =  1024; // TODO: was 4*1024, but can we get away with less since only 4 native motors are active?
     const uint16_t MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE = 0;
     const uint16_t INTERRUPT_CONTROLLER_TASK_STACKSIZE = 0;
     const uint16_t TCA_TASK_STACKSIZE = 1024;
     const uint16_t SCANNER_TASK_STACKSIZE = 0;
     const uint16_t TEMPERATURE_TASK_STACKSIZE = 0;

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = false;
     int8_t AccelStepperMotorType = 1;

     // ─────────────────────────────────────────────────────────────────────
     // Native laser drivers (PWM channels 1-4) and reserved laser id scheme
     // ─────────────────────────────────────────────────────────────────────
     // RESERVED LASER IDs ON THIS MASTER:
     //   id 0..3 → LOCAL  (driven directly via on-board PWM pins below)
     //   id 4    → REMOTE (CANopen illumination satellite board, OD sub 0x01)
     //   id 5+   → unused (kept reserved for future remote lasers)
     //
     // The CANopen Object Dictionary entry LASER_PWM_VALUE (0x2100) carries
     // 4 sub-indices, so any single REMOTE node can serve at most 4 laser
     // channels. The illumination board only has 1 PWM laser → it lives on
     // sub-axis 0 (OD sub 0x01) of node CAN_ID_LED_0 (default 30).
     int8_t LASER_1 = GPIO_NUM_12;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_2;
     // Laser id 4 is REMOTE on the illumination board; the GPIO that was
     // historically wired here is no longer driven locally. Routing is
     // enforced via ROUTE_LASER_4 below.
     int8_t LASER_4 = disabled; // was GPIO_NUM_17 — now remote (illumination board)
     int MAX_LASERS = 5+5; // Support for LASER IDs 0-4 (and reserved 5-9)
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

     // SPI (unused but configured)
     int8_t SPI_MOSI = GPIO_NUM_23;
     int8_t SPI_MISO = GPIO_NUM_19;
     int8_t SPI_SCK = GPIO_NUM_18;
     int8_t SPI_CS = GPIO_NUM_5;

     // CAN bus configuration
     int8_t CAN_RX = GPIO_NUM_33;
     int8_t CAN_TX = GPIO_NUM_32;
     uint32_t CAN_ID_CURRENT = CAN_ID_CENTRAL_NODE;

     // Set to true to send LED commands to BOTH native array AND CAN devices
     bool HYBRID_LED_DUAL_OUTPUT = false;

     // ─────────────────────────────────────────────────────────────────────
     // CANopen device routing (DeviceRouter / RoutingTable)
     // ─────────────────────────────────────────────────────────────────────
     // Make routing fully explicit so the RoutingTable does not rely on
     // pin-availability inference. This master keeps motors/lasers 0-3
     // local and forwards laser 4 + the LED array to the illumination
     // satellite board over CAN.
     int8_t ROUTE_MOTOR[4] = {0, 0, 0, 0};  // A,X,Y,Z native (FastAccelStepper) // TODO: enlarge to 10 entries?
     int8_t ROUTE_HOME[4]  = {0, 0, 0, 0};  // TODO: enlarge to 10 entries?
     int8_t ROUTE_TMC[4]   = {0, 0, 0, 0}; // TODO: enlarge to 10 entries?
     int8_t ROUTE_LASER[4] = {0, 0, 0, 0};  // lasers 0-3 native // TODO: enlarge to 10 entries?
     int8_t ROUTE_GALVO     = 1;            // remote

     // Laser id 4 → REMOTE on illumination board (CAN_ID_LED_0 = 30, sub 0x01)
     int8_t  ROUTE_LASER_4     = 1;        // REMOTE
     uint8_t CAN_NODE_LASER_4  = 30;       // = CAN_ID_LED_0
     int8_t  CAN_SUBAXIS_LASER_4 = 0;      // OD sub 0x01 on the slave

     // NeoPixel: route LED array to the illumination board as well.
     // NOTE: HYBRID_LED_DUAL_OUTPUT above is not honoured by DeviceRouter yet;
     // set ROUTE_LED = 0 to keep the on-board LED_PIN array instead.
     int8_t ROUTE_LED = 1; // REMOTE → illumination board (CAN_ID_LED_0 = 30)
};
const UC2_canopen_standalone_v4 pinConfig;
