#pragma once
// T4.4 4h: canopen_slave_gpio app-local PinConfig (converted from the old C++
// derived struct; T2.3 original: converted from
// main/config/UC2_canopen_slave_gpio/PinConfig.h — Arduino.h -> uc2_hal.h).
// T4.1 pure-C model: the board pin values now live in main/pinconfig.c
// (PINCONFIG_DEFAULTS_FIELDS + board overrides). The C++ derived struct
// `UC2_canopen_slave_gpio : PinConfig` is removed.
// Shadows the shared main/PinConfig.h (laser) for this app's build via -I main/.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif

/*
 XIAO ESP32S3 pin map for the can-gpio-esp board (values in pinconfig.c):

     D0  GPIO_NUM_1   – Remote-controllable output 1   (master writes x2301:01)
     D1  GPIO_NUM_2   – (spare)
     D2  GPIO_NUM_3   – E-stop button input            (active LOW, internal
                                                          pull-up; GPIO_ESTOP_PIN
                                                          currently disabled)
     D3  GPIO_NUM_4   – Remote-controllable output 2   (master writes x2301:02)
     D4  GPIO_NUM_5   – CAN TX
     D5  GPIO_NUM_6   – CAN RX
     D6  GPIO_NUM_43  – (spare) — also UART0 TX (Serial debug)
     D7  GPIO_NUM_44  – (spare) — also UART0 RX
     D8  GPIO_NUM_7   – (spare)
     D9  GPIO_NUM_8   – (spare)
     D10 GPIO_NUM_9   – Collision/resistance sensor ADC (ADC1_CH8)

 Notes:
     * GPIOs 1 and 4 match the user-facing identifiers in the firmware
       (digitaloutid=1 -> DIGITAL_OUT_1; id=2 -> DIGITAL_OUT_2).
     * The collision sensor is read by GpioCanSlave at ~50 Hz with a small
       EWMA filter. The trip threshold is stored in NVS preferences
       (key "gpioCT"), seeded from GPIO_COLLISION_THRESHOLD_DEFAULT
       on first boot.
*/

// ── Controller modules enabled on this slave ───────────────────────────────
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#endif
#ifndef DIGITAL_OUT_CONTROLLER
#define DIGITAL_OUT_CONTROLLER
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN
#endif
#ifndef GPIO_CAN_SLAVE_CONTROLLER
#define GPIO_CAN_SLAVE_CONTROLLER  // wires gpio_can_slave_setup/loop into main.cpp
#endif
#ifndef I2C_BRIDGE_CONTROLLER
#define I2C_BRIDGE_CONTROLLER      // generic raw-I2C passthrough (0x2350-0x2354)
#endif
