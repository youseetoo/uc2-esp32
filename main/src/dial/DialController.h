#pragma once
#include <PinConfig.h>
#include "cJSON.h"
#include <M5Unified.hpp>
#include <freertos/FreeRTOS.h>
#include <M5Dial.h>
#include "../i2c/i2c_controller.h"


struct DialData {
    // this is the data we will send via I2C from the Dial to the master 
    long pos_abs[4] = {0, 0, 0, 0}; // the absolute steps the dial has turned since boot (A, X, Y, Z)
    int qid = -1; // the qid of the dial command
};



namespace DialController
{
    int act(cJSON *jsonDocument);
    cJSON *get(cJSON *jsonDocument);
    void pullMotorPosFromDial();
    void setup();
    void loop();

    void updateDisplay();
    DialData getPositionValues();
    static DialData mPosData;

    static int ticksLastPosPulled = 0;
    static int ticksPosPullInterval = 10; // Pull position from slave every n-times

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