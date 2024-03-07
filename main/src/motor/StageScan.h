#pragma once

namespace StageScan
{
    struct StageScanningData
    {
        int nStepsLine = 100;
        int dStepsLine = 1;
        int nTriggerLine = 1;
        int nStepsPixel = 100;
        int dStepsPixel = 1;
        int nTriggerPixel = 1;
        int delayTimeStep = 10;
        int stopped = 0;
        int nFrames = 1;
    };


    void stageScan(bool isThread = false);

    StageScanningData * getStageScanData();
};