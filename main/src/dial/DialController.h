#include <PinConfig.h>
#pragma once
#include "cJSON.h"
#include <M5Unified.hpp>
#include <freertos/FreeRTOS.h>
#include <M5Dial.h>

/*
struct DialStateData
{
    // This is what we will send to the M5Stack dial from the I2C Master side
    int dial_val_0 = 0; // A Motor Pos
    int dial_val_1 = 0; // X Motor Pos
    int dial_val_2 = 0; // Y Motor Pos
    int dial_val_3 = 0; // Z Motor Pos
    int dial_state = 0; // 0 = idle, 1 = busy..
    int qid = -1;       // the qid of the dial command
};

struct DialActData
{
    // This is what we will receive from the M5Stack dial and need to parse into the different Hardware Controllers
    long pos_abs_x = 0; // the absolute steps the dial has turned since boot
    long pos_abs_y = 0; // the absolute steps the dial has turned since boot
    long pos_abs_z = 0; // the absolute steps the dial has turned since boot
    long pos_abs_a = 0; // the absolute steps the dial has turned since boot
    int qid = -1;       // the qid of the dial command
};

*/
namespace DialController
{
    int act(cJSON *jsonDocument);
    cJSON *get(cJSON *jsonDocument);
    void pullDialI2C();
    void setup();
    void loop();

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