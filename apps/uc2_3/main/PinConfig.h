#pragma once
// T3.4: UC2 Standalone V3 pin config — converted from the PIO main/config/UC2_3/PinConfig.h
// (recovered from git history; the PIO copy target was retired in T2.11). Arduino.h ->
// uc2_hal.h; feature defines wrapped in #ifndef guards because the app's CMakeLists already
// passes them as -D (a bare #define would trip -Werror).
// T4.1: board pin values moved to main/pinconfig.c (pure-C model —
// PINCONFIG_DEFAULTS_FIELDS + this board's overrides, defining the single
// extern `pinConfig` from PinConfigDefault.h). Former C++ derived struct
// `UC2_3 : PinConfig` removed.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef BLUETOOTH
#define BLUETOOTH
#endif
#ifndef BTHID
#define BTHID
#endif
#ifndef MOTOR_CONTROLLER
#define MOTOR_CONTROLLER
#endif
#ifndef USE_TCA9535
#define USE_TCA9535
#endif
#ifndef HOME_MOTOR
#define HOME_MOTOR
#endif
#ifndef LASER_CONTROLLER
#define LASER_CONTROLLER
#endif
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#endif
#ifndef LED_CONTROLLER
#define LED_CONTROLLER
#endif

/*
This is the newest electronics where direction/enable are on a seperate port extender
*/
/*
Y_Cal-Data 34
X_Cal-Data 32
Z_Cal-Data 36
I2C_SCL 22
I2C_SDA 21

SPI_MOSI 23
SPI_MISO 19
SPI_SCK 18
SPI_CS 5
X_Cal-Clk 33
Y_Cal-Clk 35
Z_CAL-CLK 17
IOexp_ int8_t 27

A_STEP 13
X_STEP 16
Y_STEP 14
Z_STEP 0
LED_1 13

PWM_1 12
PWM_2 4
PWM_3 2

In_1 - 39
*/
// UC2 STandalone V3
// https://github.com/openUC2/UC2-SUES
// 05998e057ac97c1e101c9ccc1f17070f89dd3f7c

/*
IOExpander
We are using a port extender on I2c to control
STEP_ENABLE P0
X_DIR P1
Y_DIR P2
Z_DIR P3
A_DIR P4
X_LIMIT P5
Y_LIMIT P6
Z_LIMIT P7
i2c addr should be 0x27
http://www.ti.com/lit/ds/symlink/tca9535.pdf
C561273
*/
