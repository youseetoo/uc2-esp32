#pragma once

// Include header for fixed-width integer types
#include <cstdint>

namespace StageScan
{
    static bool isRunning = false;
    
    struct StagePosition
    {
        int x;
        int y;
    };

    struct StageScanningData
    {
        // Grid-based scanning parameters
        int nStepsLine = 100;
        int dStepsLine = 1;
        int nTriggerLine = 1;
        int nStepsPixel = 100;
        int dStepsPixel = 1;
        int nTriggerPixel = 1;
        int delayTimeStep = 10;
        int stopped = 0;
        int nFrames = 1;
        int qid = -1; // query id for the task
        
        // Coordinate-based scanning parameters
        bool useCoordinates = false;
        StagePosition* coordinates = nullptr;
        int coordinateCount = 0;
        
#if defined CAN_BUS_ENABLED && !defined CAN_RECEIVE_MOTOR
        int32_t xStart = 0;
        int32_t yStart = 0;
        int32_t zStart = 0;
        int32_t xStep = 0;
        int32_t yStep = 0;
        int32_t zStep = 0;
        uint16_t nX = 0;
        uint16_t nY = 0;
        uint16_t nZ = 0;
        int delayTimePreTrigger = 0;
        int delayTimePostTrigger = 0;
        // define a boolean array of the lightsource used (e.g. [0,1,0,0,0])
        int lightsourceIntensities[5] = {0, 0, 0, 0, 0};
        int ledarrayIntensity = 0;
        int speed = 20000;
        int acceleration = 1000000;
        int delayTimeTrigger = 20; // delay time for the trigger
        bool nonstop = false; // continuous movement without stopping at each position
#endif
    };

    void stageScanThread(void *arg);
    void stageScan(bool isThread = false);
    void stageScanCAN(bool isThread = false);
    StageScanningData *getStageScanData();
    
    void setCoordinates(StagePosition* coords, int count);
    void clearCoordinates();
};