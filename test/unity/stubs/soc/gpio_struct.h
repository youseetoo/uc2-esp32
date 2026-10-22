// Host stub: soc/gpio_struct.h — HighSpeedScannerCore.cpp includes it for the
// GPIO register struct; the hardware pixel-clock register path is inactive on
// the host (no RMT), so a plain struct surface is enough.
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t out;
    uint32_t out_w1ts;
    uint32_t out_w1tc;
    uint32_t en;
    uint32_t en_w1ts;
    uint32_t en_w1tc;
    uint32_t in;
    uint32_t status;
} gpio_function_t;

// The GPIO register-block instance (IDF: soc/gpio_struct.h). Writes to its
// set/clear members compile; on the host they hit the zeroed dummy instance
// defined in host_impl.cpp (harmless no-ops).
extern gpio_function_t GPIO;
