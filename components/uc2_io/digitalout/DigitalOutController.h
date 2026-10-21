#pragma once
// T4.1: pure-C digital-output controller (was the C++ namespace
// DigitalOutController).
#include "PinConfigDefault.h"
#include <stdbool.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

int digitalout_act(cJSON *ob);
cJSON *digitalout_get(cJSON *ob);
void digitalout_setup(void);
void digitalout_loop(void);

#ifdef __cplusplus
}
#endif
