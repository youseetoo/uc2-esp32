#pragma once
// Frame order of the hardware-triggered stage scan. Pure C++11, no Arduino,
// no FreeRTOS: StageScan.cpp drives the motors/lights/trigger from these
// helpers, and test/native/test_stage_scan_order.cpp compiles this header on
// the host to pin the order. ImSwitch builds its per-frame metadata from the
// same definition (experiment_controller/scan_plan.py: stage_scan_frame_table),
// and the camera frame-id is the row index, so the two MUST stay identical:
//
//   for iy in 0..nY-1:                     rows
//     for ix in 0..nX-1:                   columns; X reversed on odd rows (zicZac)
//       for iz in 0..nZ-1:                 Z planes
//         for ch in lasers 0..4 (>0 asc), then LED (>0):   one trigger each
//         (no light configured: exactly one trigger)
#include <stdint.h>

namespace StageScanOrder
{
    static const int kLaserChannels = 5;
    static const int kChannelNone = -1;         // "no light configured" frame
    static const int kChannelLed = kLaserChannels; // the LED array, after the lasers

    // Column index actually visited for (ix, iy): odd rows run backwards when zicZac.
    inline uint16_t columnFor(uint16_t ix, uint16_t iy, uint16_t nX, bool zicZac)
    {
        return (zicZac && (iy & 1)) ? (uint16_t)(nX - 1 - ix) : ix;
    }

    // Light channels fired at one position, in order. Writes into out[] (>= 6
    // entries) and returns how many; a single kChannelNone when nothing is on.
    inline int channelSequence(const int *laserIntensities, int ledIntensity, int *out)
    {
        int n = 0;
        for (int j = 0; j < kLaserChannels; ++j)
            if (laserIntensities[j] > 0) out[n++] = j;
        if (ledIntensity > 0) out[n++] = kChannelLed;
        if (n == 0) out[n++] = kChannelNone;
        return n;
    }

    // Frames one grid scan produces: positions x planes x max(active channels, 1).
    inline uint32_t frameCount(uint16_t nX, uint16_t nY, uint16_t nZ,
                               const int *laserIntensities, int ledIntensity)
    {
        int seq[kLaserChannels + 1];
        int n = channelSequence(laserIntensities, ledIntensity, seq);
        if (seq[0] == kChannelNone) n = 1;
        return (uint32_t)nX * nY * nZ * (uint32_t)n;
    }

    // Enumerate every frame of the grid in firing order. Visitor signature:
    //   bool f(uint16_t ix, uint16_t iy, uint16_t iz, int channel)   (return false to stop)
    // where ix is the visited column (already snaked). Returns false if stopped early.
    template <typename Visitor>
    inline bool forEachFrame(uint16_t nX, uint16_t nY, uint16_t nZ, bool zicZac,
                             const int *laserIntensities, int ledIntensity, Visitor &&f)
    {
        int seq[kLaserChannels + 1];
        const int nCh = channelSequence(laserIntensities, ledIntensity, seq);
        for (uint16_t iy = 0; iy < nY; ++iy)
            for (uint16_t ix = 0; ix < nX; ++ix)
            {
                const uint16_t col = columnFor(ix, iy, nX, zicZac);
                for (uint16_t iz = 0; iz < nZ; ++iz)
                    for (int c = 0; c < nCh; ++c)
                        if (!f(col, iy, iz, seq[c])) return false;
            }
        return true;
    }
}
