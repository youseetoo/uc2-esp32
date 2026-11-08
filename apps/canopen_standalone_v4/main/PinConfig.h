#pragma once
// T6.6: UC2 canopen standalone v4 pin config (C-ified, matches the pure-C
// model from T4.1). Former C++ derived struct
// `UC2_canopen_standalone_v4 : PinConfig` replaced by pinconfig.c
// (PINCONFIG_DEFAULTS_FIELDS + board overrides).
//
// UC2 v4 Standalone board in HYBRID mode:
// - Native motors A, X, Y, Z (axes 0-3) via TCA9535
// - CAN bus motors B-G (axes 4-9)
// - Native lasers 0-3
// - CAN bus lasers 4+
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef MOTOR_CONTROLLER
#define MOTOR_CONTROLLER 1
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED 1
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN 1
#endif
#ifndef USE_FASTACCEL
#define USE_FASTACCEL 1
#endif
#ifndef USE_TCA9535
#define USE_TCA9535 1
#endif
#ifndef HOME_MOTOR
#define HOME_MOTOR 1
#endif
#ifndef LASER_CONTROLLER
#define LASER_CONTROLLER 1
#endif
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER 1
#endif
#ifndef LED_CONTROLLER
#define LED_CONTROLLER 1
#endif
#ifndef UC2_FORCE_BT_CLASSIC_ONLY
#define UC2_FORCE_BT_CLASSIC_ONLY 1
#endif
#ifndef BTHID
#define BTHID 1
#endif
#ifndef BLUETOOTH
#define BLUETOOTH 1
#endif
// Board-specific (not in the CXX_FLAGS -D set) — direct.
#define CAN_SEND_COMMANDS
#define CAN_HYBRID

// Extend motor count to include CAN satellites.
#undef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 4
