#pragma once
// T6.6: xiao_odmr app-local PinConfig (C-ified, matches the pure-C model from
// T4.1). Former C++ derived struct `UC2_ESP32S3_XIAO : PinConfig` replaced by
// pinconfig.c (PINCONFIG_DEFAULTS_FIELDS + board overrides).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO 1
#endif
#ifndef MOTOR_CONTROLLER
#define MOTOR_CONTROLLER 1
#endif
#ifndef HOME_MOTOR
#define HOME_MOTOR 1
#endif
#ifndef USE_ACCELSTEP
#define USE_ACCELSTEP 1
#endif
#ifndef LINEAR_ENCODER_CONTROLLER
#define LINEAR_ENCODER_CONTROLLER 1
#endif
#ifndef STAGE_SCAN
#define STAGE_SCAN 1
#endif
#ifndef LASER_CONTROLLER
#define LASER_CONTROLLER 1
#endif
#ifndef LED_CONTROLLER
#define LED_CONTROLLER 1
#endif
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER 1
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER 1
#endif
#ifndef HEAT_CONTROLLER
#define HEAT_CONTROLLER 1
#endif
