#pragma once
// T6.6: UC2 Standalone V4 pin config (C-ified, matches the pure-C model from
// T4.1). Former C++ derived struct `UC2_4 : PinConfig` replaced by pinconfig.c
// (PINCONFIG_DEFAULTS_FIELDS + board overrides).
//
// UC2_4 = UC2_3 flags MINUS DAC_CONTROLLER (has TCA9535).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef MOTOR_CONTROLLER
#define MOTOR_CONTROLLER 1
#endif
#ifndef USE_TCA9535
#define USE_TCA9535 1
#endif
#ifndef BLUETOOTH
#define BLUETOOTH 1
#endif
#ifndef BTHID
#define BTHID 1
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
#ifndef STAGE_SCAN
#define STAGE_SCAN 1
#endif
