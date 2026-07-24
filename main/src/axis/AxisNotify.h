#pragma once
#include <Arduino.h>

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

namespace AxisNotify
{
    struct Event
    {
        uint8_t axis           = 0;
        uint8_t fault          = 0; // AxisFault
        int32_t posErrSteps    = 0;
        int32_t commandedSteps = 0;
        int32_t measuredSteps  = 0;
    };

    // Non-blocking enqueue from the fault site. Drops the event if the queue is
    // full (a fault storm must never wedge the motor loop).
    void report(const Event &e);

    // Drain the queue (call from the main/motor loop). Cheap when empty.
    void loop();

    // UC2-specific EMCY error code for an AxisFault (manufacturer range 0xFFxx).
    uint16_t emcyCodeForFault(uint8_t fault);

    // Human-readable fault name for the JSON payload.
    const char *faultName(uint8_t fault);
}
