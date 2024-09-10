#include <PinConfig.h>
#pragma once
#include "cJSON.h"


struct DialStateData {
    // This is what we will send to the M5Stack dial from the I2C Master side
    int dial_val_0 = 0; // A Motor Pos
    int dial_val_1 = 0; // X Motor Pos
    int dial_val_2 = 0; // Y Motor Pos
    int dial_val_3 = 0; // Z Motor Pos  
    int dial_state = 0; // 0 = idle, 1 = busy..
    int qid = -1; // the qid of the dial command
};

struct DialActData {
    // This is what we will receive from the M5Stack dial and need to parse into the different Hardware Controllers
    long pos_abs_x = 0; // the absolute steps the dial has turned since boot
    long pos_abs_y = 0; // the absolute steps the dial has turned since boot
    long pos_abs_z = 0; // the absolute steps the dial has turned since boot
    long pos_abs_a = 0; // the absolute steps the dial has turned since boot
    int qid = -1; // the qid of the dial command
};

namespace DigitalInController
{
    static bool isBusy;
    static bool DEBUG = false;

    static int dial_val_1 = 0;
    static int dial_val_2 = 0;
    static int dial_val_3 = 0;

    
    int act(cJSON* jsonDocument);
    cJSON* get(cJSON* jsonDocument);
    void pullDialI2C();
    void setup();
    void loop();
};