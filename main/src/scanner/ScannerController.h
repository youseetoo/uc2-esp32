
#include "../../config.h"
#pragma once
#include "ArduinoJson.h"
//#include <FreeRTOS.h>
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "../laser/LaserController.h"
#include "../../Module.h"

class ScannerController : public Module
{
private:
    /* data */
public:
    ScannerController();
    ~ScannerController();

    bool DEBUG = false;

    bool isScanRunning = false;
    int scannerPinX = 25;
    int scannerPinY = 26;
    int scannerPinLaser = 4;

    int scannerXFrameMax = 5;
    int scannerXFrameMin = 0;
    int scannerYFrameMax = 5;
    int scannerYFrameMin = 0;
    int scannerXStep = 5;
    int scannerYStep = 5;

    int scannerxMin = 0;
    int scanneryMin = 0;
    int scannerxMax = 255;
    int scanneryMax = 255;
    int scannertDelay = 0;
    int scannerEnable = 0;
    int scannerExposure = 0;
    int scannerLaserVal = 255;
    int scannerDelay = 0;

    int scannernFrames = 1;

    int act(DynamicJsonDocument  ob) override;
    DynamicJsonDocument get(DynamicJsonDocument  ob) override;
    int set(DynamicJsonDocument ob) override;
    void setup() override;
    void loop() override;
    static void controlGalvoTask(void *parameters);
};