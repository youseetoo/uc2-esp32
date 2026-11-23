#pragma once
// Decision logic of the strobed sweep. Pure C++11, host-testable
// (test/native/test_strobe_sweep.cpp). StrobeSweep.cpp owns the hardware side.
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace StrobeSweepLogic
{
    static const uint32_t kMinPeriodUs = 2000;     // 500 frames/s ceiling
    static const uint32_t kMaxPeriodUs = 10000000; // 10 s between frames
    static const uint32_t kMaxTrigUs = 10000;
    static const int kMaxReport = 64;
    static const int kMaxAxis = 3;
    static const int kMaxLaser = 9;

    struct Params
    {
        int axis = 1;            // stepperid, A=0 X=1 Y=2 Z=3
        int32_t target = 0;      // absolute steps
        int32_t speed = 20000;   // steps/s
        int32_t accel = 0;       // 0 = firmware default
        uint32_t periodUs = 33333;
        uint32_t trigUs = 100;   // camera trigger pulse width
        int laser = -1;          // logical laser id, -1 = no flash
        int32_t delayUs = -1;    // >= 0 together with widthUs: sweep configures the strobe itself
        int32_t widthUs = -1;
        bool latch = true;       // latch the axis position on every frame
        int report = 16;         // positions per report message
        uint32_t maxFrames = 0;  // > 0: exactly this many frames, motion or not
        int qid = -1;
    };

    // nullptr when valid, otherwise a message for the host. Clamps `report`.
    inline const char *validate(Params &p)
    {
        if (p.axis < 0 || p.axis > kMaxAxis)
            return "axis must be 0..3";
        if (p.laser < -1 || p.laser > kMaxLaser)
            return "laser must be -1..9";
        if (p.trigUs < 1 || p.trigUs > kMaxTrigUs)
            return "trigUs must be 1..10000";
        if (p.periodUs < kMinPeriodUs || p.periodUs > kMaxPeriodUs)
            return "periodUs must be 2000..10000000";
        if (p.periodUs < p.trigUs + 500)
            return "periodUs must exceed trigUs by at least 500";
        if ((p.delayUs >= 0) != (p.widthUs >= 0))
            return "give both delayUs and widthUs, or neither";
        if (p.delayUs >= 0 && p.laser < 0)
            return "delayUs/widthUs need a laser";
        if (p.maxFrames == 0 && p.speed <= 0)
            return "speed must be > 0 unless maxFrames is set";
        if (p.report < 1)
            p.report = 1;
        if (p.report > kMaxReport)
            p.report = kMaxReport;
        return nullptr;
    }

    // SYNC goes over CAN when the flash or the position comes from a CAN node.
    // Otherwise (standalone board, everything local) the sweep task fires the
    // camera, the flash and the latch itself.
    inline bool needsCanSync(bool laserUsed, bool laserRemote, bool latch, bool motorRemote)
    {
        return (laserUsed && laserRemote) || (latch && motorRemote);
    }

    enum class Exit
    {
        Continue,
        Stopped,
        FramesDone,
        Arrived,
        Timeout
    };

    inline Exit exitReason(bool stopRequested, uint32_t maxFrames, uint32_t frames, bool motorArrived, bool timedOut)
    {
        if (stopRequested)
            return Exit::Stopped;
        if (maxFrames > 0)
        {
            if (frames >= maxFrames)
                return Exit::FramesDone;
        }
        else if (motorArrived)
            return Exit::Arrived;
        if (timedOut)
            return Exit::Timeout;
        return Exit::Continue;
    }

    // Advance one period; after falling behind, resync to now instead of
    // firing a burst of frames to catch up.
    inline int64_t nextTick(int64_t scheduled, int64_t now, uint32_t periodUs)
    {
        int64_t n = scheduled + (int64_t)periodUs;
        if (n <= now)
            n = now + (int64_t)periodUs;
        return n;
    }

    inline int64_t timeoutUs(uint32_t maxFrames, uint32_t periodUs, int32_t distanceSteps, int32_t speed)
    {
        if (maxFrames > 0)
            return (int64_t)maxFrames * (int64_t)periodUs + 5000000;
        const int64_t d = distanceSteps < 0 ? -(int64_t)distanceSteps : (int64_t)distanceSteps;
        const int64_t travel = speed > 0 ? d * 1000000 / speed : 0;
        return travel * 3 + 10000000;
    }

    inline bool success(bool aborted, bool error, uint32_t cameraPulses)
    {
        return !aborted && !error && cameraPulses > 0;
    }

    // Collects (frame count, position) pairs and formats them as
    // {"strobesweep":{"n":[...],"x":[...]}}.
    struct ReportBatch
    {
        uint16_t n[kMaxReport];
        int32_t x[kMaxReport];
        int count = 0;

        // True once the batch holds batchSize entries (time to send).
        bool push(uint16_t frame, int32_t position, int batchSize)
        {
            if (count < kMaxReport)
            {
                n[count] = frame;
                x[count] = position;
                ++count;
            }
            return count >= batchSize;
        }

        void clear() { count = 0; }

        // Bytes written without the terminator, or -1 when buf is too small.
        int format(char *buf, size_t len) const
        {
            size_t used = 0;
            auto put = [&](const char *fmt, long v, bool withValue) -> bool {
                const int w = withValue ? snprintf(buf + used, len - used, fmt, v)
                                        : snprintf(buf + used, len - used, "%s", fmt);
                if (w < 0 || (size_t)w >= len - used)
                    return false;
                used += (size_t)w;
                return true;
            };
            if (len == 0 || !put("{\"strobesweep\":{\"n\":[", 0, false))
                return -1;
            for (int i = 0; i < count; ++i)
                if (!put(i ? ",%ld" : "%ld", (long)n[i], true))
                    return -1;
            if (!put("],\"x\":[", 0, false))
                return -1;
            for (int i = 0; i < count; ++i)
                if (!put(i ? ",%ld" : "%ld", (long)x[i], true))
                    return -1;
            if (!put("]}}", 0, false))
                return -1;
            return (int)used;
        }
    };
}
