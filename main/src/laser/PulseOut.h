#pragma once
// One precise output pulse on a GPIO, timed by a hardware timer (legacy IDF
// timer driver: this build runs Arduino 2.0.x on ESP-IDF 4.4, which has no
// gptimer). The rising edge comes from the timer ISR after `delayUs`; pulses
// up to kBusyWaitMaxUs are ended inside the same ISR by a busy wait, so a
// 20 µs flash is exact to about a microsecond. Longer pulses end on a second
// alarm. A delay of 0 raises the pin in the caller's context.
//
// Timer allocation (timer group 1 is otherwise unused in this firmware):
//   group 1 / timer 0 : LaserStrobe flash
//   group 1 / timer 1 : StrobeSweep camera trigger
#include <stdint.h>

class PulseOut
{
public:
    static const uint32_t kBusyWaitMaxUs = 60;

    // Configures the timer and drives the pin to its inactive level.
    bool begin(int timerGroup, int timerIndex, int pin, bool activeHigh);
    // Stops the timer, leaves the pin inactive and releases it to the caller.
    void end();

    // Schedules one pulse. False when not started, still busy with the
    // previous pulse, or the width is 0.
    bool fire(uint32_t delayUs, uint32_t widthUs);

    bool started() const { return started_; }
    bool busy() const { return phase_ != Idle; }
    int pin() const { return pin_; }
    uint32_t fired() const { return fired_; }

    // ISR entry; public only so the C callback can reach it.
    bool onAlarm();

private:
    enum Phase : uint8_t
    {
        Idle,
        WaitHigh,
        WaitLow
    };

    void setActive(bool active);

    volatile Phase phase_ = Idle;
    volatile uint32_t widthUs_ = 0;
    volatile uint32_t fired_ = 0;
    int group_ = -1;
    int index_ = -1;
    int pin_ = -1;
    bool activeHigh_ = true;
    bool started_ = false;
    bool isrInstalled_ = false;
};
