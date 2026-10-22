// Stub app_main for ESP-IDF framework when test_build_src = false
// This prevents linker errors when the main application is not compiled

#ifdef UNIT_TEST
extern "C" void app_main(void) {
    // Stub - no app main needed for unit tests
}

// Host test build: provide the `pinConfig` global. On real boards each app's
// main/pinconfig.c defines it; the unit tests use the safe all-disabled
// default table (C linkage — the name is declared inside the extern "C"
// block of PinConfigDefault.h via pinconfig_defaults.h). Copy-initialization
// from the C getter (a designated-initializer list would hit C++ narrowing
// errors on the int8_t pin fields).
#include "pinconfig_defaults.h"
const struct PinConfig pinConfig = pinconfig_defaults();

#endif
