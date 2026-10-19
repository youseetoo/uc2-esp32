#pragma once
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// ATTENTION: THIS IS ONLY FOR LINTING!
// #define CORE_DEBUG_LEVEL

// #ifndef guards: the IDF build may already define board/feature flags via
// target_compile_definitions (-DFOO=1 vs empty #define FOO would -Werror).
#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif
#ifndef LASER_CONTROLLER
#define LASER_CONTROLLER
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED
#endif
#ifndef CAN_RECEIVE_LASER
#define CAN_RECEIVE_LASER
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN
#endif

// T4.1: the board pin values now live in main/pinconfig.c (pure-C model —
// PINCONFIG_DEFAULTS_FIELDS + this board's overrides, defining the single
// extern `pinConfig` from PinConfigDefault.h). Former C++ derived struct
// `UC2_canopen_slave_laser : PinConfig` removed.
//
// Pin map (Xiao ESP32S3):
//
//      D0: 1
//      D1: 2
//      D2: 3
//      D3: 4
//      D4: 5
//      D5: 6
//      D6: 43
//      D7: 44
//      D8: 7
//      D9: 8
//      D10: 9
//
//
//    This is a test to work with the UC2_3 board which acts as a I2C slave
//
// Laser pins: LASER_0..3 = D1/D2/D4/D5 (GPIO 2/3/5/6), CAN = D3/D10.
// Interlock (not wired into a controller field — no readers):
//   D9 (GPIO 8) LO when interlock has power (INTERLOCK_STATUS),
//   D0 (GPIO 1) LO when interlock tripped (INTERLOCK_LED).
// Unused (J1101 expansion): D6 (43), D7 (44), D8 (7).
