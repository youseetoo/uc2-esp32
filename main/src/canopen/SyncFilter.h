#pragma once
// Frame classification for the strobed sweep. Pure C++11: no Arduino, no
// FreeRTOS, no TWAI types, so test/native/test_sync_filter.cpp can compile it
// on the host. SyncHook.cpp feeds it the raw TWAI fields.
//
// Two frames matter:
//   SYNC  - CANopen SYNC without counter: 11-bit COB-ID from OD 0x1005
//           (default 0x080), data frame, DLC 0. Every node sees the end of
//           this frame at the same instant; that instant is the time base.
//   TPDO3 - position latch from a motor node: COB-ID 0x380 + node-id, 6 data
//           bytes = int32 step count (LE) + uint16 sync count (LE), matching
//           the OD 0x1A02 mapping (0x200C sub1, 0x200D sub1).
#include <stdint.h>

namespace SyncFilter
{
    static const uint16_t kTpdo3Base = 0x380;
    static const uint8_t kPositionFrameLen = 6;

    inline bool isSync(uint32_t id, bool extended, bool rtr, uint8_t dlc, uint16_t syncCobId)
    {
        return !extended && !rtr && dlc == 0 && (id & 0x7FFu) == (syncCobId & 0x7FFu);
    }

    inline void encodePosition(int32_t position, uint16_t count, uint8_t *d)
    {
        const uint32_t p = (uint32_t)position;
        d[0] = (uint8_t)(p & 0xFF);
        d[1] = (uint8_t)((p >> 8) & 0xFF);
        d[2] = (uint8_t)((p >> 16) & 0xFF);
        d[3] = (uint8_t)((p >> 24) & 0xFF);
        d[4] = (uint8_t)(count & 0xFF);
        d[5] = (uint8_t)((count >> 8) & 0xFF);
    }

    // True when the frame is a position latch; fills nodeId/position/count.
    inline bool decodePosition(uint32_t id, bool extended, bool rtr, uint8_t dlc, const uint8_t *d,
                               uint8_t &nodeId, int32_t &position, uint16_t &count)
    {
        if (extended || rtr || dlc < kPositionFrameLen || d == nullptr)
            return false;
        const uint16_t cob = (uint16_t)(id & 0x7FFu);
        if ((cob & 0x780u) != kTpdo3Base)
            return false;
        const uint8_t node = (uint8_t)(cob & 0x7Fu);
        if (node == 0)
            return false;
        nodeId = node;
        position = (int32_t)((uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24));
        count = (uint16_t)((uint16_t)d[4] | ((uint16_t)d[5] << 8));
        return true;
    }

    // Fixed-capacity callback list. add() is idempotent; remove() clears the
    // slot. Callers serialise add/remove against dispatch themselves (SyncHook
    // snapshots the list under a spinlock).
    template <typename Fn, int N>
    struct CallbackList
    {
        Fn fn[N] = {};
        void *ctx[N] = {};

        bool add(Fn f, void *c)
        {
            for (int i = 0; i < N; ++i)
                if (fn[i] == f && ctx[i] == c)
                    return true;
            for (int i = 0; i < N; ++i)
                if (fn[i] == nullptr)
                {
                    ctx[i] = c;
                    fn[i] = f;
                    return true;
                }
            return false;
        }

        void remove(Fn f, void *c)
        {
            for (int i = 0; i < N; ++i)
                if (fn[i] == f && ctx[i] == c)
                {
                    fn[i] = nullptr;
                    ctx[i] = nullptr;
                }
        }

        int size() const
        {
            int n = 0;
            for (int i = 0; i < N; ++i)
                if (fn[i] != nullptr)
                    ++n;
            return n;
        }
    };
}
