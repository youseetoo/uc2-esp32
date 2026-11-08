#pragma once
// T6.6: canopen_slave_galvo app-local PinConfig (C-ified, matches the pure-C
// model from T4.1). Former C++ derived struct `UC2_canopen_slave_galvo : PinConfig`
// replaced by pinconfig.c (PINCONFIG_DEFAULTS_FIELDS + board overrides).
//
// Galvo scanner board (XIAO ESP32-S3): MCP4822 dual 12-bit SPI DAC (X/Y mirror
// positioning) + RMT 3-trigger system (pixel/line/frame) + laser-enable GPIO.
// CAN node id: CAN_ID_GALVO_0 (single galvo slave).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif
#ifndef GALVO_CONTROLLER
#define GALVO_CONTROLLER 1
#endif
#ifndef CAN_RECEIVE_GALVO
#define CAN_RECEIVE_GALVO 1
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED 1
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN 1
#endif
