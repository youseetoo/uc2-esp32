// T4.1: pure C (was the C++ StageScan namespace).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sentinel "leave this axis alone" for the per-coordinate "z" (C replacement
// for the old StageScan::kKeepAxis).
#define STAGE_SCAN_KEEP_AXIS (-1)

typedef struct StagePosition
{
    int x;
    int y;
    int z; // STAGE_SCAN_KEEP_AXIS = don't move the Z axis for this coordinate
} StagePosition;

typedef struct StageScanningData
{
    // Grid-based scanning parameters
    int xStart;
    int yStart;
    int xStep;
    int yStep;
    int zStart;
    int zStep;
    int nX;
    int nY;
    int nZ;

    int delayTimeStep;
    int delayTimeTrigger;
    int delayTimePreTrigger;
    int delayTimePostTrigger;

    bool zicZac;
    int  speed;        // steps /s
    int  acceleration; // steps /s^2

    // Coordinate-based scanning parameters
    bool useCoordinates;
    StagePosition *coordinates; // malloc'd; owned by this struct
    int coordinateCount;

    int nFrames;
    int stopped;
    int qid;
    int nonstop; // 1 = continuous-row / fly-through scan, 0 = stop-and-go

    int lightsourceIntensities[5]; // per-laser channel intensity
    int ledarrayIntensity;         // LED array intensity (0-255)
} StageScanningData;

// C replacement for the old NSDMI defaults (delayTimeStep=10, nFrames=1);
// the rest is zero.
static inline StageScanningData stage_scan_data_init(void)
{
    StageScanningData d = {0};
    d.delayTimeStep = 10;
    d.nFrames = 1;
    return d;
}

StageScanningData *stage_scan_get_data(void);
void stage_scan_set_coordinates(StagePosition *coords, int count);
void stage_scan_clear_coordinates(void);
void stage_scan(bool isThread); // isThread => vTaskDelete(NULL) on exit
void stage_scan_thread(void *arg); // FreeRTOS entry (see MotorJsonParser::parseStageScan)

extern volatile bool stage_scan_is_running;

#ifdef __cplusplus
}
#endif
