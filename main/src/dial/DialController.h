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
} DialData;


namespace DialController
{
    int act(cJSON *jsonDocument);
    cJSON *get(cJSON *jsonDocument);
    void pullMotorPosFromDial();
    void setup();
    void loop();

    void writeTextDisplay(String text);
    void updateDisplay();
    DialData getPositionValues();
    void sendMotorPosI2C();
    static DialData mPosData;
    
    static int ticksLastPosPulled = 0;
    static int ticksPosPullInterval = 50; // Pull position from slave every n-times

    // Array to store X, Y, Z, A positions
    static long positions[4] = {0, 0, 0, 0};
    static char axisNames[4] = {'X', 'Y', 'Z', 'A'};
    static int currentAxis = 0; // 0=X, 1=Y, 2=Z, 3=A
    static int stepSize = 1;    // Step size starts at 1
    static long encoderPos = -999;

    // Available touch states
    static const int TOUCH_BEGIN = 3;      // Represents touch start
    static const int TOUCH_HOLD_BEGIN = 7; // Represents touch hold start

    static unsigned long touchStartTime = 0;
    static const unsigned long LONG_PRESS_DURATION = 150; // 1 second for long press


};