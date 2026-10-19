// T4.1: C shim body for the C++ DigitalInController E-stop API (see
// digitalin_c.h) — consumed by the pure-C State (uc2_core).
#include "digitalin_c.h"
#include "DigitalInController.h"

extern "C" void digitalin_c_set_emergency_polarity(int8_t pol)
{
    DigitalInController::setEmergencyPolarity(pol);
}

extern "C" int8_t digitalin_c_get_emergency_polarity(void)
{
    return DigitalInController::getEmergencyPolarity();
}

extern "C" int digitalin_c_get_emergency_raw(void)
{
    return DigitalInController::getEmergencyRaw();
}

extern "C" bool digitalin_c_is_emergency_active(void)
{
    return DigitalInController::isEmergencyActive();
}
