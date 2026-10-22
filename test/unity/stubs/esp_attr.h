// Host stub: esp_attr.h — attribute macros are no-ops on the host.
#pragma once
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif
#ifndef ESP_UNUSED
#define ESP_UNUSED(x) (void)(x)
#endif
