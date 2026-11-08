#pragma once
// T6.6: waveshare_d7 (Waveshare ESP32-S3 7" screen board) pin config (C-ified,
// matches the pure-C model from T4.1). Former C++ derived struct
// `waveshare_esp32s3_7inchScreen : PinConfig` replaced by pinconfig.c
// (PINCONFIG_DEFAULTS_FIELDS + board overrides).
//
// Despite the T5.4 "M5GFX" grouping, this is a CAN MOTOR node: the config was
// copied from the XIAO can-slave-motor board (single X motor + TMC UART +
// CAN_ID_MOT_X). No display pins, no M5GFX/M5Unified.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO 1
#endif
#ifndef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 10
#endif
