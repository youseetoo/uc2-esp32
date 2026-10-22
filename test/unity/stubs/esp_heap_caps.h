// Host stub: esp_heap_caps.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t esp_get_free_heap_size(void);
uint32_t esp_get_free_internal_heap_size(void);
uint32_t esp_get_minimum_free_heap_size(void);
uint32_t esp_get_free_heap_size_static(void);

#ifdef __cplusplus
}
#endif
