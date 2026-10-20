#pragma once
// T4.1: pure C (was a C++ namespace).
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// EncoderMonitor — periodic, thread-safe encoder liveness monitor.
//
// Purpose: verify the encoder is actually counting, live, without attaching a
// debugger. AxisController::loop() feeds it the count it already sampled each
// iteration (no extra PCNT access); at the configured period the monitor
//   1. updates a snapshot readable from ANY task (short critical section), and
//   2. pushes a compact JSON report on the serial channel:
//        {"encoderMonitor":{"axis":1,"count":1234,"delta":56,"dtMs":500}}
//
// Enable per axis with /motor_act: {"steppers":[{"stepperid":1,"encmonitor":500}]}
// (period in ms, clamped to >= 50; 0 disables). Off by default.
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EncoderMonitorSnapshot {
    int64_t  count;        // count at the last completed interval
    int64_t  delta;        // change over that interval
    uint32_t sampleMs;     // millis() when it was taken
    uint32_t intervalMs;   // actual elapsed time of the interval
    bool     valid;        // at least one full interval observed
} EncoderMonitorSnapshot;

// Enable/disable periodic reporting. periodMs == 0 disables; otherwise
// clamped to >= 50 ms so the serial channel can't be flooded.
void encoder_monitor_set_period(int axis, uint32_t periodMs);
uint32_t encoder_monitor_get_period(int axis);

// Feed one sample. Called from AxisController::loop() with the raw count it
// already read this iteration. Emits the JSON report when the period lapses.
void encoder_monitor_tick(int axis, int64_t rawCounts, uint32_t nowMs);

// Thread-safe read of the latest completed interval, callable from any task.
EncoderMonitorSnapshot encoder_monitor_get_snapshot(int axis);

#ifdef __cplusplus
}
#endif
