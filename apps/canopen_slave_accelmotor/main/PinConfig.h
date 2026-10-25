#pragma once
// T4.3: canopen_slave_accelmotor app-local PinConfig (converted from the old
// C++ derived struct). T4.1 pure-C model: board pin values now live in
// main/pinconfig.c (PINCONFIG_DEFAULTS_FIELDS + board overrides). The C++
// derived struct `UC2_canopen_slave_motor : PinConfig` is removed.
//
// Single-axis (X) stage-motor CANopen slave (XIAO ESP32-S3): AccelStepper
// stepper X (STEP/DIR/EN GPIO8/7/9), TMC2209 driver over UART (GPIO43/44),
// linear encoder (ENC_X_A/B GPIO5/6, PCNT), hard limit (DIGITAL_IN_1 GPIO1),
// CAN (GPIO2/3). MOTOR_AXIS_COUNT=10 — the axis framework allocates per-axis
// state; only X is wired on this board.
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
#ifndef USE_ACCELSTEP
#define USE_ACCELSTEP
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
