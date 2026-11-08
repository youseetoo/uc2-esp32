#pragma once
// T6.6: canopen_bridge_ptz app-local PinConfig (C-ified, matches the pure-C
// model from T4.1). Former C++ derived struct
// `UC2_canopen_bridge_ptz : PinConfig` replaced by pinconfig.c
// (PINCONFIG_DEFAULTS_FIELDS + board overrides).
//
// RS-485 Pelco-D PTZ keyboard (UART1) -> CANopen SDO bridge; all actuators REMOTE.
// Node-id 61 (remote actor, right after GPIO slave at 60).
//
// CAN: RX 2, TX 3
// RS-485: RX 5, TX 6
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif

// ── Controller modules enabled on this bridge ───────────────────────────────
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER 1
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED 1
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN 1
#endif
#ifndef PTZ_KEYBOARD_CONTROLLER
#define PTZ_KEYBOARD_CONTROLLER 1
#endif
