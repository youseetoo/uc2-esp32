#include <PinConfig.h>
#pragma once
#include <stdbool.h>
#include "cJSON.h"

#ifdef __cplusplus
namespace DigitalInController
{
    // T1.3: per-TU namespace statics — not every includer uses every member.
    [[maybe_unused]] static bool isDEBUG = false;

    [[maybe_unused]] static int digitalin_val_1 = 0;
    [[maybe_unused]] static int digitalin_val_2 = 0;
    [[maybe_unused]] static int digitalin_val_3 = 0;

    int act(cJSON* jsonDocument);
    cJSON* get(cJSON* jsonDocument);
    bool getDigitalVal(int digitalinid);
    void setup();
    void loop();

    // ── Emergency-STOP (pinEmergencyExit) ────────────────────────────────
    // Safety path. Polled via checkEmergencyStop() directly from the main
    // loop, independently of the runtimeConfig.digitalIn flag, so it stays
    // active even when generic digital-input polling is disabled. The first
    // call lazily configures the pin — there is no separate setup step.
    void checkEmergencyStop();
    bool isEmergencyActive();

    // Configurable E-stop polarity (asserted logic level): 0=active-LOW,
    // 1=active-HIGH. Persisted in NVS, applied live. getEmergencyRaw() returns
    // the current raw pin level (HIGH/LOW, or -1 if unconfigured) for tuning.
    void   setEmergencyPolarity(int8_t pol);
    int8_t getEmergencyPolarity();
    int    getEmergencyRaw();
};
#endif // __cplusplus

// T4.1: C shim for pure-C modules (HomeMotor.c) that read a digital input.
#ifdef __cplusplus
extern "C" {
#endif
bool digitalin_get_val(int digitalinid);
#ifdef __cplusplus
}
#endif
