#pragma once
// T6.6: canopen_bridge_ps4_usbhost app-local PinConfig (C-ified, matches the
// pure-C model from T4.1). Former C++ derived struct
// `UC2_canopen_bridge_ps4_usbhost : PinConfig` replaced by pinconfig.c
// (PINCONFIG_DEFAULTS_FIELDS + board overrides, defining the single
// extern `pinConfig` from PinConfigDefault.h).
//
// DS4 gamepad over USB-OTG (GPIO19/20, fixed) -> expedited CANopen SDO bridge;
// all actuators REMOTE. Node-id 5 (remote actor).
//
// I2C: SDA 6, SCL 7
// CAN: RX 14, TX 13
// USB: D- 20, D+ 19
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif

// ── Controller modules enabled on this bridge ───────────────────────────────
// Same five as the PIO env (UC2_canopen_xiao_base + release): the bridge runs
// the CANopen stack (expedited SDO writes, RoutingTable) and serial messaging,
// with the DS4 USB-host provider on top. No actuator controllers.
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER 1
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED 1
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN 1
#endif
#ifndef JOYSTICK_USBHOST_PROVIDER
#define JOYSTICK_USBHOST_PROVIDER 1
#endif
