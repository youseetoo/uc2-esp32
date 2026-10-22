// Host stub: rom/crc.h
//
// Real software CRC32 (little-endian polynomial 0xEDB88320) — the same
// algorithm as the ESP32 ROM routine, so host-computed digests match what
// the device would produce. static inline so it works from C and C++ TUs.
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t crc32_le(uint32_t seed, const uint8_t* data, size_t len) {
    uint32_t crc = seed;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    return crc;
}

#ifdef __cplusplus
}
#endif
