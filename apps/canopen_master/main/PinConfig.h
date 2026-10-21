#pragma once
// T3.5: UC2 canopen_master pin config — converted from the PIO
// main/config/UC2_canopen_master/PinConfig.h (recovered from git history; the PIO
// copy target was retired in T2.11). Arduino.h -> uc2_hal.h; the feature #defines
// are wrapped in #ifndef guards because the app's CMakeLists already passes them
// as -D (a bare #define would trip -Werror on redefinition).
// T4.1: board pin values moved to main/pinconfig.c (pure-C model —
// PINCONFIG_DEFAULTS_FIELDS + this board's overrides, defining the single
// extern `pinConfig` from PinConfigDefault.h). Former C++ derived struct
// `UC2_canopen_master : PinConfig` removed.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef LASER_CONTROLLER
#define LASER_CONTROLLER 1
#endif
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER 1
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER 1
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED 1
#endif
#ifndef DIAL_CONTROLLER
#define DIAL_CONTROLLER 1
#endif
#ifndef MOTOR_CONTROLLER
#define MOTOR_CONTROLLER 1
#endif
#ifndef HOME_MOTOR
#define HOME_MOTOR 1
#endif
#ifndef BTHID
#define BTHID 1
#endif
#ifndef BLUETOOTH
#define BLUETOOTH 1
#endif
#ifndef TMC_CONTROLLER
#define TMC_CONTROLLER 1
#endif
#ifndef STAGE_SCAN
#define STAGE_SCAN 1
#endif
#ifndef LED_CONTROLLER
#define LED_CONTROLLER 1
#endif
#ifndef GALVO_CONTROLLER
#define GALVO_CONTROLLER 1
#endif
// T4.1: DAC_CONTROLLER removed — the PIO-only DacController was never in the
// IDF tree (uc2_app CMakeLists T4.1 note), so the feature is dead here.
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN 1
#endif
#ifndef FAN_CONTROLLER
#define FAN_CONTROLLER 1
#endif
#ifndef TMP102_CONTROLLER
#define TMP102_CONTROLLER 1
#endif

// Board-specific (not passed via the app -D set):
#define CAN_SEND_COMMANDS
// Override the base PinConfigDefault.h default (4) — the master routes 10 axes.
#undef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 10

/*
Former C++ derived-struct fields with no readers anywhere (grep-verified) —
pin map kept for reference. Board values that DID have readers are in
main/pinconfig.c.

I2C (on-board device addresses, TODO: surface via sensor_act/sensor_get):
  INA226  0x46 (current sensor)
  TMP102  0x4A (ambient), 0x4B (on-board)
  ALERT lines (open-drain, not wired): INA226 n/c, TMP102_1 n/c, TMP102_2 n/c
  pinALERT = GPIO_NUM_35 (TODO: temperature-sensor alert)

Camera / trigger / panelboard IO:
  CAM_IO0_IN  = GPIO_NUM_27  (used as CAMERA_TRIGGER_PIN in pinconfig.c)
  CAM_IO1_OUT = disabled     ("Cam Out Line 1")
  CAM_IO2_IO  = disabled     ("Cam In/Out Line 2")

Encoder / motor direction flags (readers dropped in T4.1, like UC2_3):
  ENC_A_encoderDirection = true
  ENC_X/Y/Z_motorDirection = true

Objective defaults (no readers on the master's CAN-routed path):
  objectivePositionX0 = 5000
  objectivePositionX1 = 35000
  objectiveHomeDirection = -1
  objectiveHomeEndStopPolarity = 0
*/
