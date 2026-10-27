#pragma once
// Configure the strobe of a *logical* laser id wherever it lives, following
// the RoutingTable like every other laser command:
//   LOCAL  -> LaserStrobe on this board
//   REMOTE -> SDO writes to OD 0x2107..0x2109 on the owning CAN node, then a
//             read-back to confirm the node accepted the configuration
// A node with older firmware answers the probe read of 0x2107 with an SDO
// abort; that is reported as "no strobe support" instead of failing silently.
#include <stdint.h>
#include "cJSON.h"

namespace StrobeRoute
{
    struct Result
    {
        bool ok = false;        // request applied
        bool supported = false; // the target can strobe at all
        bool remote = false;    // configured over CAN
        bool enabled = false;   // strobe state after the request
        uint8_t nodeId = 0;     // CAN node when remote
        uint32_t delayUs = 0;   // values as applied (clamped by the node)
        uint32_t widthUs = 0;
        uint32_t count = 0;     // flashes since the last enable
        const char *error = nullptr;
    };

    Result configure(int laserId, bool enable, uint32_t delayUs, uint32_t widthUs);
    Result status(int laserId);
    bool isRemote(int laserId);

    // {"supported":1,"enabled":1,"remote":1,"node":30,"delayUs":..,"widthUs":..,"count":..}
    cJSON *toJson(const Result &r);
}
