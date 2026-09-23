#pragma once

// Hardware-triggered stage scan: the firmware walks a grid (or a coordinate
// list), switches the lights and fires the camera trigger; the host only
// collects frames. The frame order is defined in StageScanOrder.h and mirrored
// by ImSwitch (scan_plan.stage_scan_frame_table) — keep them in sync.
#include <cstdint>

namespace StageScan
{
    // Set while a scan task runs. One definition (StageScan.cpp): it used to
    // be a header-static, so the parser's "already running" guard read its
    // own always-false copy and a second command spawned a second scan task.
    extern volatile bool isRunning;

    // Coordinate value meaning "leave this axis where it is" (coordinate mode
    // entries without a "z").
    static const int32_t kKeepAxis = INT32_MIN;

    struct StagePosition
    {
        int x;
        int y;
        int z;
    };

    struct StageScanningData
    {
        // Legacy line/pixel fields (unused by the grid scan, kept for callers)
        int nStepsLine = 100;
        int dStepsLine = 1;
        int nTriggerLine = 1;
        int nStepsPixel = 100;
        int dStepsPixel = 1;
        int nTriggerPixel = 1;
        int delayTimeStep = 10;
        int stopped = 0;
        int nFrames = 1;
        int qid = -1; // query id echoed in the completion message
        bool zicZac = true;

        // Coordinate-based scanning parameters
        bool useCoordinates = false;
        StagePosition* coordinates = nullptr;
        int coordinateCount = 0;

        // Grid: 0 for a start means "from the current position"
        int32_t xStart = 0;
        int32_t yStart = 0;
        int32_t zStart = 0;
        int32_t xStep = 0;
        int32_t yStep = 0;
        int32_t zStep = 0;
        uint16_t nX = 1;
        uint16_t nY = 1;
        uint16_t nZ = 1;
        int delayTimePreTrigger = 0;   // tPre  [ms]: settle after the move / light on
        int delayTimePostTrigger = 0;  // tPost [ms]: exposure after the trigger
        // Laser channels 0..4 (illumination[] in the JSON), fired ascending when > 0
        int lightsourceIntensities[5] = {0, 0, 0, 0, 0};
        int ledarrayIntensity = 0;     // LED array, fired after the lasers when > 0
        int speed = 20000;
        int acceleration = 1000000;
        int delayTimeTrigger = 1;      // tTrig [ms]: camera trigger pulse width (>= 1)
        bool nonstop = false;          // sweep rows continuously, trigger on the fly
    };

    void stageScanThread(void *arg);
    void stageScan(bool isThread = false);
    StageScanningData *getStageScanData();

    void setCoordinates(StagePosition* coords, int count);
    void clearCoordinates();
};
