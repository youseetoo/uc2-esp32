#pragma once
// Microsecond-class SYNC handling for the strobed sweep.
//
// The CANopen stack only sees received frames through CO_interrupt_task,
// which polls every 1 ms at priority 2, so any hook inside the stack inherits
// up to 1 ms of jitter. CANopenModule::CAN_ctrl_task (priority 5) wakes on the
// TWAI hardware alert and sees every raw frame first; it calls into this
// module before handing the frame to the stack, which still processes SYNC
// as before. Nothing here changes behaviour unless a callback is registered.
//
//   node side   : addRxCallback()   - SYNC received (flash, position latch)
//   master side : addTxCallback()   - own SYNC finished transmitting (camera)
//                 setPositionCallback() - TPDO3 position latch from a node
#include <stdint.h>
#include "driver/twai.h"

namespace SyncHook
{
    typedef void (*SyncCallback)(int64_t tUs, void *ctx);
    typedef void (*PositionCallback)(uint8_t nodeId, int32_t position, uint16_t count, void *ctx);

    bool addRxCallback(SyncCallback cb, void *ctx);
    void removeRxCallback(SyncCallback cb, void *ctx);
    bool addTxCallback(SyncCallback cb, void *ctx);
    void removeTxCallback(SyncCallback cb, void *ctx);
    void setPositionCallback(PositionCallback cb, void *ctx); // nullptr clears

    // Queue a SYNC frame at the front of the TX queue (master). The TX-success
    // alert of that frame triggers the TX callbacks.
    bool sendSync();

    // COB-ID of SYNC from OD 0x1005 (default 0x080).
    uint16_t syncCobId();

    uint32_t syncsReceived();
    uint32_t syncsTransmitted();

    // --- called by CANopenModule::CAN_ctrl_task only ---
    void onFrameReceived(const twai_message_t &m);
    void onFrameTransmitted(const twai_message_t &m);
    void onTxSuccess();
    void onTxFailed();
}
