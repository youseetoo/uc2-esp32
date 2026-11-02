#pragma once
// Strobed constant-velocity sweep (master / standalone board).
//
// {"task":"/motor_act","strobesweep":{"axis":1,"target":250000,"speed":20000,
//   "periodUs":33333,"trigUs":100,"laser":4,"delayUs":1200,"widthUs":20,
//   "latch":1,"report":16,"maxFrames":0}}
// {"task":"/motor_act","strobesweep":{"stopped":1}}
//
// Per frame the camera trigger pulses and, when a laser is given, the flash
// fires delayUs later for widthUs. If the flash or the position comes from a
// CAN node, the frame is a SYNC frame: the camera pulses when the SYNC has
// left the controller, the illumination node flashes relative to its SYNC
// reception and the motor node latches its position (TPDO3). Otherwise the
// sweep task fires everything locally.
//
// Reports while running: {"strobesweep":{"n":[1,2,...],"x":[steps,...]}}
// Completion: {"strobesweep":true,"frames":N,"camera":N,"flashes":N|-1,
//   "positions":N,"latch":0|1,"strobe":0|1,"aborted":0|1,"success":0|1,
//   "qid":q[,"error":"..."]}
#include "cJSON.h"

namespace StrobeSweep
{
    void parseJson(cJSON *doc); // handles the "strobesweep" key of /motor_act
    bool isRunning();
    void stop();
}
