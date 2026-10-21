#pragma once
// TCA9535 I/O expander (UC2_2/3/4/wemos boards, gated by USE_TCA9535).
//
// T4.1: pure-C API. The implementation (tca_controller.cpp) is a small C++
// island around the vendored RobTillaart TCA9555 class (components/tca9555)
// + the FreeRTOS mutex; all state (incl. the TCA9535 object) is file-static
// in the .cpp, so there is exactly ONE initialized device per binary — the
// old header-scoped `static TCA9535 TCA` gave each includer its own
// uninitialized copy.
//
//   tca_init()               — Wire.begin (if needed) + TCA.begin + pin modes + seeds
//   tca_set_external_pin()   — FastAccelStepper external-pin callback (handshake)
//   tca_read_endstop()       — inverted level read of an endstop pin
//
// Without USE_TCA9535 the functions are no-op/pass-through (see .cpp).

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void tca_init(void);
bool tca_set_external_pin(uint8_t pin, uint8_t value);
bool tca_read_endstop(uint8_t pin);

#ifdef __cplusplus
}
#endif
