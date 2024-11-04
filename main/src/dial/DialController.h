#pragma once
#include <PinConfig.h>
#include "cJSON.h"
#include <freertos/FreeRTOS.h>
#include "../i2c/i2c_controller.h"

#ifdef I2C_SLAVE
#include <M5Unified.hpp>
#include <M5Dial.h>
#endif 
typedef struct __attribute__((packed))
{
    int32_t pos_x = 0;
    int32_t pos_y = 0;
    int32_t pos_z = 0;
    int32_t pos_a = 0;
    int32_t intensity = 0;
} DialData;

namespace DialController
{
    int act(cJSON *jsonDocument);
    cJSON *get(cJSON *jsonDocument);
    void pullParamsFromDial();
    void setup();
    void loop();

    // Display functions
    void writeTextDisplay(String text);
    void updateDisplay();

    // Motor data
    DialData getDialValues();
    void setDialValues(DialData mDialData);
    static DialData mDialData;

    static int ticksLastPosPulled = 0;
    static int ticksPosPullInterval = 5; // Pull position from slave every n-times
    static bool positionsPushedToDial = false;

    // Array to store X, Y, Z, A positions
    static long values[5] = {0, 0, 0, 0, 0};
    static char axisNames[5] = {'A', 'X', 'Y', 'Z', 'I'};
    static int currentAxis = 0; // 0=X, 1=Y, 2=Z, 3=A; 4=Intensity
    static int stepSize = 1;    // Step size starts at 1
    static long encoderPos = 0;
    static int lastIntensity = 0;

    // Available touch states
    static const int TOUCH_BEGIN = 3;      // Represents touch start
    static const int TOUCH_HOLD_BEGIN = 7; // Represents touch hold start

    static unsigned long touchStartTime = 0;
    static const unsigned long LONG_PRESS_DURATION = 150; // 1 second for long press


};