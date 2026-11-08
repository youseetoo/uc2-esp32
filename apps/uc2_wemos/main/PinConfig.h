#pragma once
// T6.6: UC2 Standalone on Wemos D1 R32 pin config (C-ified, matches the pure-C
// model from T4.1). Former C++ derived struct `UC2_WEMOS : PinConfig` replaced
// by pinconfig.c (PINCONFIG_DEFAULTS_FIELDS + board overrides).
//
// TCA/DAC/DS18B20 pins are not wired on this board's minimal header set, so
// they fall back to the PinConfigDefault.h defaults (disabled / GPIO_NUM_NC).
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER
