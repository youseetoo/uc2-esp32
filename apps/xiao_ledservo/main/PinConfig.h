#pragma once
// T6.6: xiao_ledservo app-local PinConfig (C-ified, matches the pure-C model
// from T4.1). Former C++ derived struct `UC2_ESP32S3_XIAO_LEDSERVO : PinConfig`
// replaced by pinconfig.c (PINCONFIG_DEFAULTS_FIELDS + board overrides).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO 1
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
