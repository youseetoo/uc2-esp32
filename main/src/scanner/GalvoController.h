#pragma once

#include <PinConfig.h>
#include "SPIRenderer.h"
#include "cJSON.h"
#include "Arduino.h"

// TODO: need to update this function to match the new framework
namespace  GalvoController  
{
    static bool isGalvoScanRunning = false;
    static int SLAVE_ADDR = 0x28; // Slave address, must match the slave's address
    static int X_MIN = 0;
    static int X_MAX = 30000;
    static int Y_MIN = 0;
    static int Y_MAX = 30000;
    static int STEP = 1000;
    static int tPixelDwelltime = 1;
    static int nFrames = 10;
    static bool fastMode = true; // Enable fast mode by default

    static SPIRenderer *renderer; 

    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
    void setup();
    void loop();
    void writeInt(int);
    void setFastMode(bool enabled);
  };