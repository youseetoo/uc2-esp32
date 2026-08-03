#include "EncoderMonitor.h"
#include "../serial/SerialProcess.h"
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "EncMonitor";

namespace EncoderMonitor
{
    static constexpr int MAX_AXES = 4;
    static constexpr uint32_t MIN_PERIOD_MS = 50;

    struct AxisMon
    {
        // Configuration + accumulation — touched only by the feeding task
        // (AxisController::loop) except periodMs, which is a single aligned
        // 32-bit word (atomic on Xtensa) written from the command path.
        volatile uint32_t periodMs = 0; // 0 = off
        bool     primed = false;        // have a reference sample yet?
        int64_t  refCount = 0;          // count at interval start
        uint32_t refMs = 0;             // millis() at interval start
        Snapshot snap;                  // shared: guarded by s_mux
    };

    static AxisMon s_mon[MAX_AXES];
    static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

    void setPeriod(int axis, uint32_t periodMs)
    {
        if (axis < 0 || axis >= MAX_AXES)
            return;
        if (periodMs > 0 && periodMs < MIN_PERIOD_MS)
            periodMs = MIN_PERIOD_MS;
        s_mon[axis].periodMs = periodMs;
        s_mon[axis].primed = false; // restart the interval cleanly
        ESP_LOGI(TAG, "axis %d encoder monitor %s (period %u ms)", axis,
                 periodMs ? "ON" : "OFF", (unsigned)periodMs);
    }

    uint32_t getPeriod(int axis)
    {
        if (axis < 0 || axis >= MAX_AXES)
            return 0;
        return s_mon[axis].periodMs;
    }

    Snapshot getSnapshot(int axis)
    {
        Snapshot out;
        if (axis < 0 || axis >= MAX_AXES)
            return out;
        portENTER_CRITICAL(&s_mux);
        out = s_mon[axis].snap;
        portEXIT_CRITICAL(&s_mux);
        return out;
    }

    static void emitReport(int axis, const Snapshot &s)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON *m = cJSON_AddObjectToObject(root, "encoderMonitor");
        cJSON_AddNumberToObject(m, "axis", axis);
        cJSON_AddNumberToObject(m, "count", (double)s.count);
        cJSON_AddNumberToObject(m, "delta", (double)s.delta);
        cJSON_AddNumberToObject(m, "dtMs", s.intervalMs);
        char *json = cJSON_PrintUnformatted(root);
        if (json)
        {
            SerialProcess::safeSendJsonString(json);
            free(json);
        }
        cJSON_Delete(root);
    }

    void tick(int axis, int64_t rawCounts, uint32_t nowMs)
    {
        if (axis < 0 || axis >= MAX_AXES)
            return;
        AxisMon &a = s_mon[axis];
        uint32_t period = a.periodMs;
        if (period == 0)
        {
            a.primed = false;
            return;
        }
        if (!a.primed)
        {
            a.refCount = rawCounts;
            a.refMs = nowMs;
            a.primed = true;
            return;
        }
        uint32_t elapsed = nowMs - a.refMs;
        if (elapsed < period)
            return;

        Snapshot s;
        s.count = rawCounts;
        s.delta = rawCounts - a.refCount;
        s.sampleMs = nowMs;
        s.intervalMs = elapsed;
        s.valid = true;

        portENTER_CRITICAL(&s_mux);
        a.snap = s;
        portEXIT_CRITICAL(&s_mux);

        a.refCount = rawCounts;
        a.refMs = nowMs;

        emitReport(axis, s);
    }
}
