#pragma once
// T6.6: UC2 Standalone V2 pin config (C-ified, matches the pure-C model from
// T4.1). Former C++ derived struct `UC2_2 : PinConfig` replaced by pinconfig.c
// (PINCONFIG_DEFAULTS_FIELDS + board overrides).
//
// No TCA9535 on this board (env UC2_2 has no -DUSE_TCA9535).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER
