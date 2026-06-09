#include <PinConfig.h>
#pragma once
#include "cJSON.h"


namespace DigitalInController
{
    static bool isDEBUG = false;

    static int digitalin_val_1 = 0;
    static int digitalin_val_2 = 0;
    static int digitalin_val_3 = 0;

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
