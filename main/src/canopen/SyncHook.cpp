#include "SyncHook.h"
#include "SyncFilter.h"
#include "CANopenModule.h"
#include <CANopen.h>
#include "OD.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

namespace SyncHook
{
    typedef SyncFilter::CallbackList<SyncCallback, 4> List;

    static List s_rx;
    static List s_tx;
    static PositionCallback volatile s_posCb = nullptr;
    static void *s_posCtx = nullptr;
    static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

    static volatile bool s_syncInFlight = false;
    static volatile uint32_t s_rxCount = 0;
    static volatile uint32_t s_txCount = 0;

    bool addRxCallback(SyncCallback cb, void *ctx)
    {
        portENTER_CRITICAL(&s_mux);
        const bool ok = s_rx.add(cb, ctx);
        portEXIT_CRITICAL(&s_mux);
        return ok;
    }

    void removeRxCallback(SyncCallback cb, void *ctx)
    {
        portENTER_CRITICAL(&s_mux);
        s_rx.remove(cb, ctx);
        portEXIT_CRITICAL(&s_mux);
    }

    bool addTxCallback(SyncCallback cb, void *ctx)
    {
        portENTER_CRITICAL(&s_mux);
        const bool ok = s_tx.add(cb, ctx);
        portEXIT_CRITICAL(&s_mux);
        return ok;
    }

    void removeTxCallback(SyncCallback cb, void *ctx)
    {
        portENTER_CRITICAL(&s_mux);
        s_tx.remove(cb, ctx);
        portEXIT_CRITICAL(&s_mux);
    }

    void setPositionCallback(PositionCallback cb, void *ctx)
    {
        portENTER_CRITICAL(&s_mux);
        s_posCtx = ctx;
        s_posCb = cb;
        portEXIT_CRITICAL(&s_mux);
    }

    uint16_t syncCobId()
    {
        return (uint16_t)(OD_PERSIST_COMM.x1005_COB_ID_SYNCMessage & 0x7FFu);
    }

    uint32_t syncsReceived() { return s_rxCount; }
    uint32_t syncsTransmitted() { return s_txCount; }

    // Snapshot under the lock, call outside it: a callback may take longer
    // than a critical section is allowed to last.
    static void dispatch(const List &list, int64_t t)
    {
        List snap;
        portENTER_CRITICAL(&s_mux);
        snap = list;
        portEXIT_CRITICAL(&s_mux);
        for (int i = 0; i < 4; ++i)
            if (snap.fn[i] != nullptr)
                snap.fn[i](t, snap.ctx[i]);
    }

    static inline bool isSyncMsg(const twai_message_t &m)
    {
        return SyncFilter::isSync(m.identifier, (m.flags & TWAI_MSG_FLAG_EXTD) != 0,
                                  (m.flags & TWAI_MSG_FLAG_RTR) != 0, m.data_length_code, syncCobId());
    }

    void onFrameReceived(const twai_message_t &m)
    {
        if (isSyncMsg(m))
        {
            const int64_t t = esp_timer_get_time();
            s_rxCount = s_rxCount + 1;
            dispatch(s_rx, t);
            return;
        }

        // Every received frame passes here; without an active sweep there is
        // no position callback and no reason to take the lock.
        if (s_posCb == nullptr)
            return;
        PositionCallback cb;
        void *ctx;
        portENTER_CRITICAL(&s_mux);
        cb = s_posCb;
        ctx = s_posCtx;
        portEXIT_CRITICAL(&s_mux);
        if (cb == nullptr)
            return;

        uint8_t node;
        int32_t pos;
        uint16_t count;
        if (SyncFilter::decodePosition(m.identifier, (m.flags & TWAI_MSG_FLAG_EXTD) != 0,
                                       (m.flags & TWAI_MSG_FLAG_RTR) != 0, m.data_length_code, m.data,
                                       node, pos, count))
            cb(node, pos, count, ctx);
    }

    static void fireTx()
    {
        s_syncInFlight = false;
        s_txCount = s_txCount + 1;
        dispatch(s_tx, esp_timer_get_time());
    }

    void onFrameTransmitted(const twai_message_t &m)
    {
        // The TX loop only hands the controller a frame when the hardware
        // buffer is idle. If a SYNC is still marked in flight, it therefore
        // completed but its TX-success alert has not been read yet: fire now
        // rather than lose the camera pulse when the flag is overwritten.
        if (s_syncInFlight)
            fireTx();
        s_syncInFlight = isSyncMsg(m);
    }

    void onTxSuccess()
    {
        if (s_syncInFlight)
            fireTx();
    }

    void onTxFailed()
    {
        s_syncInFlight = false;
    }

    bool sendSync()
    {
        return CANopenModule::sendRawFrameFront(syncCobId(), 0, nullptr);
    }
}
