#pragma once
// Position latch for the strobed sweep.
//
// Motor slave: on every received SYNC, while enabled (OD 0x200E), take the
// live step count of the local axis, store it with a running count in OD
// 0x200C/0x200D and request TPDO3 (COB-ID 0x380 + node-id, 6 bytes). The
// count starts at 1 for the first SYNC after enabling, so count n belongs to
// frame n of the sweep.
//
// Master/standalone: livePosition() gives the same number for a local axis.
#include <stdint.h>

namespace SyncLatch
{
    // Motor slaves only, before canopenModule.setup(): make TPDO3 valid and
    // register the SYNC callback. It never transmits until enabled.
    void setupSlave();

    void setEnabled(bool enabled); // enabling resets the count
    bool enabled();
    uint16_t count();

    int32_t livePosition(int axis);
}
