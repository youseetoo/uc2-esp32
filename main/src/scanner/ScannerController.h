#include <PinConfig.h>
#ifdef SCANNER_CONTROLLER
#pragma once
#include "cJSON.h"


namespace ScannerController
{
    static bool DEBUG = false;

    static bool isScanRunning = false;
    static int scannerPinX = 25;
    static int scannerPinY = 26;
    static int scannerPinLaser = 4;

    static int scannerXFrameMax = 5;
    static int scannerXFrameMin = 0;
    static int scannerYFrameMax = 5;
    static int scannerYFrameMin = 0;
    static int scannerXStep = 5;
    static int scannerYStep = 5;

    static int scannerxMin = 0;
    static int scanneryMin = 0;
    static int scannerxMax = 255;
    static int scanneryMax = 255;
    static int scannertDelay = 0;
    static int scannerEnable = 0;
    static int scannerExposure = 0;
    static int scannerLaserVal = 255;
    static int scannerDelay = 0;

    static int scannernFrames = 1;

    int act(cJSON * ob);
    void setup();
    void loop();
    void controlGalvoTask(void *parameters);
};
#endif