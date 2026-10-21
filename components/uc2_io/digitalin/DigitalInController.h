#pragma once
// T4.1: pure-C digital-input controller (was the C++ namespace
// DigitalInController). The E-stop API replaced the old digitalin_c.* shim.
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

int digitalin_act(cJSON *jsonDocument);
cJSON *digitalin_get(cJSON *jsonDocument);
bool digitalin_get_val(int digitalinid);
void digitalin_setup(void);
void digitalin_loop(void);

// ── Emergency-STOP (pinEmergencyExit) ────────────────────────────────────
// Safety path. Polled via digitalin_check_emergency_stop() directly from the
// main loop, independently of the runtimeConfig.digitalIn flag, so it stays
// active even when generic digital-input polling is disabled. The first call
// lazily configures the pin — there is no separate setup step.
void digitalin_check_emergency_stop(void);
bool digitalin_is_emergency_active(void);

// Configurable E-stop polarity (asserted logic level): 0=active-LOW,
// 1=active-HIGH. Persisted in NVS, applied live. digitalin_get_emergency_raw()
// returns the current raw pin level (HIGH/LOW, or -1 if unconfigured) for
// tuning.
void   digitalin_set_emergency_polarity(int8_t pol);
int8_t digitalin_get_emergency_polarity(void);
int    digitalin_get_emergency_raw(void);

#ifdef __cplusplus
}
#endif
