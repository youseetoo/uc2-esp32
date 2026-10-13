#pragma once
// T2.2: waveshare_ledarray app-local PinConfig (converted from
// main/config/waveshare_esp32s3_ledarray/PinConfig.h: Arduino.h -> uc2_hal.h).
// Shadows the shared main/PinConfig.h (laser) for this app's build via -I main/.
// Per-app PinConfig rework lands in T2.11.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// #ifndef guards: the IDF build may already define board/feature flags via
// CMAKE_CXX_FLAGS (-DFOO=1 vs an empty #define FOO would trip -Werror).
#ifndef WAVESHARE_ESP32S3_LEDARRAY
#define WAVESHARE_ESP32S3_LEDARRAY
#endif
#ifndef LED_CONTROLLER
#define LED_CONTROLLER
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif

struct UC2_ESP32S3_WAVESHARE_LEDARRAY : PinConfig
{
    const char *pindefName = "UC2_esp32s3_waveshare_ledarray";
    const unsigned long BAUDRATE = 115200;

    int8_t LED_PIN = GPIO_NUM_14;
    int8_t LED_COUNT = 64;

    bool dumpHeap = false;

    const uint16_t serialTimeout = 100;

    uint32_t CAN_ID_CURRENT = disabled;
};

const UC2_ESP32S3_WAVESHARE_LEDARRAY pinConfig;
