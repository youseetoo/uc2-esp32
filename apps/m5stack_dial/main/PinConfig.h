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
//
// T4.1: Board pin values moved to main/pinconfig.c (pure-C model —
// PINCONFIG_DEFAULTS_FIELDS + this board's overrides, defining the single
// extern `pinConfig` from PinConfigDefault.h). Former C++ derived struct
// `UC2_M5StackDial : PinConfig` removed.
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