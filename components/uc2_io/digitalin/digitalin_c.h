#pragma once
// T4.1: C shim for the C++ DigitalInController E-stop API — used by the
// pure-C State (uc2_core). The C State.c calls these C functions; this shim
// (a thin C++ TU in uc2_io) forwards to the C++ DigitalInController
// namespace. Keeps the C module free of C++ dependencies. Weak no-op
// defaults live in State.c for builds without uc2_io's digitalin.
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void digitalin_c_set_emergency_polarity(int8_t pol);
int8_t digitalin_c_get_emergency_polarity(void);
int digitalin_c_get_emergency_raw(void);
bool digitalin_c_is_emergency_active(void);

#ifdef __cplusplus
}
#endif
