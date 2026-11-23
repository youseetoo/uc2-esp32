#pragma once
// Strobe engine for a local laser/LED channel: one flash per trigger, timed
// from the trigger instant by a hardware timer (PulseOut, timer group 1/0).
//
// On a CANopen node the trigger is the reception of a SYNC frame (SyncHook
// RX callback, registered on the first enable). On a standalone board the
// sweep task calls fireFrom() directly. While a channel strobes, its pin is
// detached from the LEDC PWM peripheral and held inactive between flashes;
// LaserController::setLaserVal() stores new PWM values but does not apply
// them, and disabling the strobe restores the stored PWM value.
//
// Only one channel can strobe at a time (one hardware timer). Limits come
// from pinConfig (STROBE_MAX_WIDTH_US, STROBE_MAX_DELAY_US,
// STROBE_MAX_DUTY_PERMILLE) and are enforced here, not only by the host.
#include <stdint.h>

namespace LaserStrobe
{
    // Enable/disable/update one local laser id. On failure returns false and
    // sets *err (static string). Values are clamped; read them back below.
    bool configure(int laserId, bool enable, uint32_t delayUs, uint32_t widthUs, const char **err);

    bool isStrobing(int laserId);
    int activeChannel(); // -1 when none

    // Arm the flash of the active channel relative to a trigger time
    // (esp_timer microseconds). Called from the SYNC hook or the sweep task.
    void fireFrom(int64_t tTriggerUs);

    uint32_t delayUs(int laserId);
    uint32_t widthUs(int laserId);
    uint32_t fired(int laserId);
    uint32_t skipped(int laserId);
}
