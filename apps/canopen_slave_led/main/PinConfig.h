#pragma once
// T6.6: canopen_slave_led app-local PinConfig (C-ified, matches the pure-C
// model from T4.1). Former C++ derived struct `UC2_canopen_slave_led : PinConfig`
// replaced by pinconfig.c (PINCONFIG_DEFAULTS_FIELDS + board overrides).
//
// UC2 Illumination Board (CANopen slave, XIAO ESP32-S3): NeoPixel ring stack
// (4 concentric rings, 136 LEDs total) + single high-power white LED driven by
// PWM (exposed as logical laser 0) + heat-sink thermistors (rev. H).
// Single CANopen node ID (CAN_ID_LED_0, default 30) covers BOTH the LED ring
// and the PWM laser.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif
#ifndef LASER_CONTROLLER
#define LASER_CONTROLLER 1
#endif
#ifndef LED_CONTROLLER
#define LED_CONTROLLER 1
#endif
#ifndef THERMAL_CONTROLLER
#define THERMAL_CONTROLLER 1
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER 1
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED 1
#endif
#ifndef CAN_RECEIVE_LASER
#define CAN_RECEIVE_LASER 1
#endif
#ifndef CAN_RECEIVE_LED
#define CAN_RECEIVE_LED 1
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN 1
#endif
