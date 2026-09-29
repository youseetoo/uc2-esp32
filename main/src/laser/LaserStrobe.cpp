#include <PinConfig.h>
#include "LaserStrobe.h"
#include "LaserController.h"
#include "PulseOut.h"
#include "StrobeTiming.h"
#include "Arduino.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#ifdef CAN_CONTROLLER_CANOPEN
#include "../canopen/SyncHook.h"
#include <CANopen.h>
#include "OD.h"
#endif

namespace LaserStrobe
{
    static const int kTimerGroup = 1;
    static const int kTimerIndex = 0;

    static StrobeTiming::Channel s_ch[LaserController::MAX_LASERS];
    static PulseOut s_pulse;
    static volatile int s_active = -1;
    static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
#ifdef CAN_CONTROLLER_CANOPEN
    static bool s_hookRegistered = false;
#endif

    static StrobeTiming::Limits limits()
    {
        StrobeTiming::Limits L;
        L.maxWidthUs = pinConfig.STROBE_MAX_WIDTH_US;
        L.maxDelayUs = pinConfig.STROBE_MAX_DELAY_US;
        L.maxDutyPermille = pinConfig.STROBE_MAX_DUTY_PERMILLE;
        return L;
    }

    static bool validId(int id) { return id >= 0 && id < LaserController::MAX_LASERS; }

    static void publishCount(int id, uint32_t n)
    {
#ifdef CAN_CONTROLLER_CANOPEN
        if (id >= 0 && id < 4)
            OD_RAM.x210A_laser_strobe_count[id] = n;
#else
        (void)id;
        (void)n;
#endif
    }

#ifdef CAN_CONTROLLER_CANOPEN
    static void onSync(int64_t tUs, void *)
    {
        fireFrom(tUs);
    }
#endif

    void fireFrom(int64_t tTriggerUs)
    {
        const int id = s_active;
        if (!validId(id))
            return;
        StrobeTiming::Channel &c = s_ch[id];
        if (!c.enabled)
            return;
        const int64_t start = tTriggerUs + (int64_t)c.delayUs;
        if (!c.duty.allow(start, c.widthUs, pinConfig.STROBE_MAX_DUTY_PERMILLE))
        {
            c.skipped++;
            return;
        }
        const uint32_t rem = StrobeTiming::remainingDelay(c.delayUs, tTriggerUs, esp_timer_get_time());
        if (!s_pulse.fire(rem, c.widthUs))
        {
            c.skipped++;
            return;
        }
        c.fired++;
        publishCount(id, c.fired);
    }

    static void restorePwm(int id, int pin)
    {
        const int chan = LaserController::getPWMChannel(id);
        if (pin <= 0 || chan < 0)
            return;
        ledcAttachPin(pin, chan);
        ledcWrite(chan, LaserController::getLaserVal(id));
    }

    bool configure(int laserId, bool enable, uint32_t delayUs, uint32_t widthUs, const char **err)
    {
        const char *dummy;
        if (err == nullptr)
            err = &dummy;
        *err = nullptr;
        if (!validId(laserId))
        {
            *err = "invalid laser id";
            return false;
        }
        const int pin = LaserController::getLaserPin(laserId);
        if (enable && pin <= 0)
        {
            *err = "this laser id has no pin on this board";
            return false;
        }
        if (enable && s_active >= 0 && s_active != laserId)
        {
            *err = "another laser channel is already strobing (one at a time)";
            return false;
        }

        StrobeTiming::Channel &c = s_ch[laserId];
        portENTER_CRITICAL(&s_mux);
        const StrobeTiming::Change change = StrobeTiming::apply(c, enable, delayUs, widthUs, limits());
        portEXIT_CRITICAL(&s_mux);

        switch (change)
        {
        case StrobeTiming::Change::Enable:
            // Take the pin from the PWM peripheral; PulseOut drives it as a
            // plain GPIO and holds it inactive between flashes.
            ledcDetachPin(pin);
            if (!s_pulse.begin(kTimerGroup, kTimerIndex, pin, true))
            {
                c.enabled = false;
                restorePwm(laserId, pin);
                *err = "strobe timer unavailable";
                return false;
            }
            publishCount(laserId, 0);
            s_active = laserId;
#ifdef CAN_CONTROLLER_CANOPEN
            if (!s_hookRegistered)
                s_hookRegistered = SyncHook::addRxCallback(onSync, nullptr);
#endif
            log_i("Strobe on: laser %d, pin %d, delay %lu us, width %lu us", laserId, pin,
                  (unsigned long)c.delayUs, (unsigned long)c.widthUs);
            break;

        case StrobeTiming::Change::Disable:
        {
            s_active = -1;
            // Let a flash that is already armed finish (bounded by the delay and width limits).
            const int64_t until = esp_timer_get_time() + (int64_t)pinConfig.STROBE_MAX_DELAY_US + 2000;
            while (s_pulse.busy() && esp_timer_get_time() < until)
                vTaskDelay(1);
            s_pulse.end();
            restorePwm(laserId, pin);
            log_i("Strobe off: laser %d, %lu flashes, %lu skipped", laserId, (unsigned long)c.fired,
                  (unsigned long)c.skipped);
            break;
        }

        case StrobeTiming::Change::Update:
            log_i("Strobe update: laser %d, delay %lu us, width %lu us", laserId, (unsigned long)c.delayUs,
                  (unsigned long)c.widthUs);
            break;

        case StrobeTiming::Change::None:
            break;
        }
        return true;
    }

    bool isStrobing(int laserId) { return validId(laserId) && s_ch[laserId].enabled; }
    int activeChannel() { return s_active; }
    uint32_t delayUs(int laserId) { return validId(laserId) ? s_ch[laserId].delayUs : 0; }
    uint32_t widthUs(int laserId) { return validId(laserId) ? s_ch[laserId].widthUs : 0; }
    uint32_t fired(int laserId) { return validId(laserId) ? s_ch[laserId].fired : 0; }
    uint32_t skipped(int laserId) { return validId(laserId) ? s_ch[laserId].skipped : 0; }
}
