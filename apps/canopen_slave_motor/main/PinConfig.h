#pragma once
// T2.7: canopen_slave_motor app-local PinConfig (converted from
// main/config/UC2_canopen_slave_motor/PinConfig.h: Arduino.h -> uc2_hal.h).
// Shadows the shared main/PinConfig.h for this app's build via
// UC2_APP_MAIN_DIR (uc2_modinc.cmake prepends it). Per-app PinConfig rework
// lands in T2.11.
//
// Multi-axis (A/X/Y/Z) stage-motor CANopen slave (XIAO ESP32-S3): FAS
// (FastAccelStepper, RMT backend) stepper X (STEP/DIR/EN GPIO8/7/9), TMC over
// UART (GPIO43/44), linear encoder (ENC_X_A/B GPIO40/41, PCNT), hard limit
// (DIGITAL_IN_1 GPIO1), CAN (GPIO2/3). MOTOR_AXIS_COUNT=10 — the axis
// framework allocates per-axis state; only X is wired on this board.
#include "uc2_hal.h"
#include "PinConfigDefault.h"

#undef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 10
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif
#ifndef CAN_RECEIVE_MOTOR
#define CAN_RECEIVE_MOTOR
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED
#endif
#ifndef MOTOR_CONTROLLER
#define MOTOR_CONTROLLER
#endif
#ifndef HOME_MOTOR
#define HOME_MOTOR
#endif
#ifndef USE_FASTACCEL
#define USE_FASTACCEL
#endif
#ifndef TMC_CONTROLLER
#define TMC_CONTROLLER
#endif
#ifndef LINEAR_ENCODER_CONTROLLER
#define LINEAR_ENCODER_CONTROLLER
#endif
#ifndef AXIS_CONTROLLER
#define AXIS_CONTROLLER
#endif
#ifndef USE_PCNT_COUNTER
#define USE_PCNT_COUNTER
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN
#endif

struct UC2_canopen_slave_motor : PinConfig
{
    const char *pindefName = "UC2_canopen_slave_motor";
    const unsigned long BAUDRATE = 921600;

    bool DEBUG_CAN_ISO_TP = 0; // 1 = debug CAN communication, 0 = no debug

    int8_t MOTOR_X_STEP = GPIO_NUM_8;  // D9 -> GPIO8
    int8_t MOTOR_X_DIR = GPIO_NUM_7;   // D8 -> GPIO7
    int8_t MOTOR_ENABLE = GPIO_NUM_9;  // D10 -> GPIO9
    bool MOTOR_ENABLE_INVERTED = true;
    bool MOTOR_AUTOENABLE = false;
    int8_t AccelStepperMotorType = 1;

    // I2c - as slave
    const char *I2C_NAME = "MOTX";
    int8_t I2C_ADD_SLAVE = I2C_ADD_MOT_X;    // I2C address if it's a slave
    int8_t I2C_SCL = disabled;
    int8_t I2C_SDA = disabled;
    // I2C - as controller
    int8_t I2C_SCL_ext = disabled;
    int8_t I2C_SDA_ext = disabled;

    // TMC UART
    int8_t tmc_SW_RX = 44;    // D7 -> GPIO44
    int8_t tmc_SW_TX = 43;    // D6 -> GPIO43
    int8_t tmc_pin_diag = GPIO_NUM_4; // D3 -> GPIO4
    bool TMC_DEBUG = false;
    int tmc_microsteps = 16;
    int tmc_rms_current = 1400;
    int tmc_stall_value = 50;
    int tmc_sgthrs = 5;
    int tmc_semin = 0;
    int tmc_semax = 0;
    int tmc_sedn = 0;
    int tmc_tcoolthrs = 0xFFFFF;
    int tmc_blank_time = 24;
    int tmc_toff = 4;

    // CAN
    int8_t CAN_TX = GPIO_NUM_3;  // D2 in (I2C SDA) CAN Motor Board
    int8_t CAN_RX = GPIO_NUM_2;  // D1 in (I2C SCL) CAN Motor Board
#ifndef CAN_AXIS_ID
#define CAN_AXIS_ID CAN_ID_MOT_X
#endif
    uint32_t CAN_ID_CURRENT = CAN_AXIS_ID;

    // Routing overrides: 0=LOCAL, 1=REMOTE, 2=OFF (-1=infer)
    int8_t ROUTE_MOTOR[4] = {2, 0, 2, 2}; // A=OFF, X=LOCAL, Y=OFF, Z=OFF
    int8_t ROUTE_HOME[4]  = {2, 0, 2, 2};
    int8_t ROUTE_TMC[4]   = {2, 0, 2, 2};
    int8_t ROUTE_LASER[4] = {2, 2, 2, 2}; // all OFF on this slave
    int8_t ROUTE_LED       = 2;            // OFF
    int8_t ROUTE_GALVO     = 2;            // OFF

    // Endstop
    int8_t DIGITAL_IN_1 = GPIO_NUM_1; // D0 -> GPIO1 -> TOUCH
    uint32_t objectivePositionX0 = 10000;
    uint32_t objectivePositionX1 = 80000;

    // Linear encoder pins (AS5311 interface)
    int8_t ENC_X_A = GPIO_NUM_40; // linear_A
    int8_t ENC_X_B = GPIO_NUM_41; // linear_B
    int8_t ENC_Y_A = disabled;
    int8_t ENC_Y_B = disabled;
    int8_t ENC_Z_A = disabled;
    int8_t ENC_Z_B = disabled;
    bool ENC_X_encoderDirection = true;
    bool ENC_Y_encoderDirection = true;
    bool ENC_Z_encoderDirection = true;

    // limit settings
    bool hardLimitEnabledX = true;
    bool hardLimitPolarityX = true;
    // Homing back-off: steps to move OFF the endstop (after it releases) before
    // setting home=0, so homing never ends sitting on the switch.
    uint16_t homeEndstopReleaseSteps = 50;

    // OTA settings
    const uint32_t OTA_TIME_FROM_STARTUP = 30000; // 30 seconds

    const uint16_t serialTimeout = 100;
};

const UC2_canopen_slave_motor pinConfig;
