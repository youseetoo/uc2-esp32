// T4.1: EncoderMonitor — pure C (was a C++ namespace).
static const char *TAG = "EncMonitor";
#include "EncoderMonitor.h"
#include "uc2_serial_out.h"   // serial_out_safe_send_json_string (C API)
#include "cJSON.h"
#include "uc2_hal/uc2_log.h"
#include "freertos/FreeRTOS.h" // portMUX
#include <stdlib.h>

#define ENCMON_MAX_AXES 4
#define ENCMON_MIN_PERIOD_MS 50u

typedef struct AxisMon {
    // Configuration + accumulation — touched only by the feeding task
    // (AxisController::loop) except periodMs, which is a single aligned
    // 32-bit word (atomic on Xtensa) written from the command path.
    volatile uint32_t periodMs; // 0 = off
    bool     primed;            // have a reference sample yet?
    int64_t  refCount;          // count at interval start
    uint32_t refMs;             // millis() at interval start
    EncoderMonitorSnapshot snap; // shared: guarded by s_mux
} AxisMon;

static AxisMon s_mon[ENCMON_MAX_AXES];
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

void encoder_monitor_set_period(int axis, uint32_t periodMs)
{
    if (axis < 0 || axis >= ENCMON_MAX_AXES)
        return;
    if (periodMs > 0 && periodMs < ENCMON_MIN_PERIOD_MS)
        periodMs = ENCMON_MIN_PERIOD_MS;
    s_mon[axis].periodMs = periodMs;
    s_mon[axis].primed = false; // restart the interval cleanly
    log_i("axis %d encoder monitor %s (period %u ms)", axis,
          periodMs ? "ON" : "OFF", (unsigned)periodMs);
}

uint32_t encoder_monitor_get_period(int axis)
{
    if (axis < 0 || axis >= ENCMON_MAX_AXES)
        return 0;
    return s_mon[axis].periodMs;
}

EncoderMonitorSnapshot encoder_monitor_get_snapshot(int axis)
{
    EncoderMonitorSnapshot out = {0};
    if (axis < 0 || axis >= ENCMON_MAX_AXES)
        return out;
    portENTER_CRITICAL(&s_mux);
    out = s_mon[axis].snap;
    portEXIT_CRITICAL(&s_mux);
    return out;
}

static void encoder_monitor_emit_report(int axis, const EncoderMonitorSnapshot *s)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *m = cJSON_AddObjectToObject(root, "encoderMonitor");
    cJSON_AddNumberToObject(m, "axis", axis);
    cJSON_AddNumberToObject(m, "count", (double)s->count);
    cJSON_AddNumberToObject(m, "delta", (double)s->delta);
    cJSON_AddNumberToObject(m, "dtMs", s->intervalMs);
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        serial_out_safe_send_json_string(json);
        free(json);
    }
    cJSON_Delete(root);
}

void encoder_monitor_tick(int axis, int64_t rawCounts, uint32_t nowMs)
{
    if (axis < 0 || axis >= ENCMON_MAX_AXES)
        return;
    AxisMon *a = &s_mon[axis];
    uint32_t period = a->periodMs;
    if (period == 0)
    {
        a->primed = false;
        return;
    }
    if (!a->primed)
    {
        a->refCount = rawCounts;
        a->refMs = nowMs;
        a->primed = true;
        return;
    }
    uint32_t elapsed = nowMs - a->refMs;
    if (elapsed < period)
        return;

    EncoderMonitorSnapshot s;
    s.count = rawCounts;
    s.delta = rawCounts - a->refCount;
    s.sampleMs = nowMs;
    s.intervalMs = elapsed;
    s.valid = true;

    portENTER_CRITICAL(&s_mux);
    a->snap = s;
    portEXIT_CRITICAL(&s_mux);

    a->refCount = rawCounts;
    a->refMs = nowMs;

    encoder_monitor_emit_report(axis, &s);
}
