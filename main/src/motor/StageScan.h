#pragma once

namespace StageScan
{
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
        
        // Coordinate-based scanning parameters
        bool useCoordinates = false;
        StagePosition* coordinates = nullptr;
        int coordinateCount = 0;
    };

    void stageScan(bool isThread = false);

    StageScanningData * getStageScanData();
    
    void setCoordinates(StagePosition* coords, int count);
    void clearCoordinates();
};