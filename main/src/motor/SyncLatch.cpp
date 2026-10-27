#include <PinConfig.h>
#include "SyncLatch.h"
#include "FocusMotor.h"
#include "Arduino.h"
#ifdef CAN_CONTROLLER_CANOPEN
#include "../canopen/SyncHook.h"
#include "../canopen/CANopenModule.h" // extern CO_t* CO
#include <CANopen.h>
#include "OD.h"
#endif

namespace SyncLatch
{
    static volatile bool s_enabled = false;
    static volatile uint16_t s_count = 0;

    int32_t livePosition(int axis)
    {
        MotorData *d = FocusMotor::getData()[axis];
        if (d == nullptr)
            return 0;
        // updateData() pulls the stepper driver's current position (bounds
        // checked for remote/absent axes) into the data array.
        FocusMotor::updateData(axis);
        return d->currentPosition;
    }

#ifdef CAN_CONTROLLER_CANOPEN
    static void onSync(int64_t, void *)
    {
        if (!s_enabled)
            return;
        const int32_t pos = livePosition((int)pinConfig.REMOTE_MOTOR_AXIS_ID);
        const uint16_t n = (uint16_t)(s_count + 1);
        s_count = n;
        // Single-motor slave: all four sub-indices carry the same value, like
        // TPDO1. The mapping reads sub 1.
        for (int ax = 0; ax < 4; ++ax)
        {
            OD_RAM.x200C_motor_sync_position[ax] = pos;
            OD_RAM.x200D_motor_sync_count[ax] = n;
        }
        if (CO != NULL && CO->TPDO != NULL)
            CO_TPDOsendRequest(&CO->TPDO[2]); // TPDO3 = index 2
    }
#endif

    void setupSlave()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        // Bit 31 clear = valid; 0x380 is the stack's predefined TPDO3 base,
        // so CANopenNode adds the node-id at activation -> 0x380 + id.
        OD_PERSIST_COMM.x1802_TPDOCommunicationParameter.COB_IDUsedByTPDO = 0x40000380u;
        SyncHook::addRxCallback(onSync, nullptr);
        log_i("SyncLatch: TPDO3 position latch armed (idle until 0x200E = 1)");
#endif
    }

    void setEnabled(bool en)
    {
        if (en && !s_enabled)
            s_count = 0;
        s_enabled = en;
        log_i("SyncLatch %s", en ? "enabled" : "disabled");
    }

    bool enabled() { return s_enabled; }
    uint16_t count() { return s_count; }
}
