#pragma once
// T6.6: waveshare_ledarray app-local PinConfig (C-ified, matches the pure-C
// model from T4.1). Former C++ derived struct
// `UC2_ESP32S3_WAVESHARE_LEDARRAY : PinConfig` replaced by pinconfig.c
// (PINCONFIG_DEFAULTS_FIELDS + board overrides).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef WAVESHARE_ESP32S3_LEDARRAY
#define WAVESHARE_ESP32S3_LEDARRAY 1
#endif
#ifndef LED_CONTROLLER
#define LED_CONTROLLER 1
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER 1
#endif
