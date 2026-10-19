#pragma once
// T4.1: State — converted from a C++ namespace to a pure-C module so it can
// be built by a C compiler and linked from C call sites.
//
// The former per-TU header statics are now single shared globals defined in
// State.c. Side effect: State's `isBusy` was documented as "TODO this is not
// working!!!" precisely because each translation unit had its own copy
// (LaserController wrote one, SerialProcessIn read another); it is now truly
// shared, so /state_get isBusy reflects the laser busy flag.
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/* formerly per-TU header statics — now single shared globals (State.c) */
extern bool state_isdebug;
extern const char *state_identifier_name;    // "UC2_Feather"
extern const char *state_identifier_id;      // "V2.0"
extern const char *state_identifier_date;    // build __DATE__ __TIME__
extern const char *state_identifier_author;  // "BD"
extern const char *state_identifier_product; // "uc2-esp" (was IDENTIFIER_NAME)
extern bool state_config_set;

/* timing variables */
extern unsigned long state_start_millis;
extern unsigned long state_current_millis;
extern bool state_is_busy; // TODO this is not working!!! (now actually shared)
extern bool state_otarunning;

int state_act(cJSON *ob);
cJSON *state_get(cJSON *ob);
cJSON *state_get_modules(void);

void state_setup(void);
void state_print_info(void);

void state_set_busy(bool busy);
bool state_get_busy(void);

/* CAN-bus power control (BUSPOWER_OFF_PIN gate; HIGH = OFF, LOW = ON).
 * Default ON at boot. Exposed via /state_act {"power":0|1} and /state_get.
 * state_set_bus_power is also called by the E-stop handler to cut all slave
 * power. */
void state_set_bus_power(bool on);
bool state_get_bus_power(void);

void state_start_ota(void);
void state_stop_ota(void);

#ifdef __cplusplus
}
#endif
