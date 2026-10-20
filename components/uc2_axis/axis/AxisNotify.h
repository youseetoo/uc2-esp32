#pragma once
// T4.1: pure C (was a C++ namespace).
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// AxisNotify — asynchronous fault notification (design v2, WP5).
//
// Fault sites (the watchdog, verify/correct, calibration) call report() — a
// fire-and-forget enqueue that never blocks on CAN TX. loop() drains the queue:
//   - on a CAN slave  -> emit a CANopen EMCY (0x80+node) via CANopenModule
//   - standalone/serial -> emit the {"axisEvent":{...}} JSON directly
// The master turns inbound EMCY into the same JSON (see CANopenModule).
//
// All values are in STEPS.
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AxisEvent {
    uint8_t axis;             // logical axis index
    uint8_t fault;            // AxisFault
    int32_t posErrSteps;
    int32_t commandedSteps;
    int32_t measuredSteps;
} AxisEvent;

// Non-blocking enqueue from the fault site. Drops the event if the queue is
// full (a fault storm must never wedge the motor loop).
void axis_notify_report(const AxisEvent *e);

// Drain the queue (call from the main/motor loop). Cheap when empty.
void axis_notify_loop(void);

// UC2-specific EMCY error code for an AxisFault (manufacturer range 0xFFxx).
uint16_t axis_notify_emcy_code_for_fault(uint8_t fault);

// Human-readable fault name for the JSON payload.
const char *axis_notify_fault_name(uint8_t fault);

#ifdef __cplusplus
}
#endif
