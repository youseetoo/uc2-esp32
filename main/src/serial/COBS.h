#pragma once
#include <stdint.h>
#include <stddef.h>

// Lightweight COBS (Consistent Overhead Byte Stuffing) encoder/decoder.
// Frame format:  0x00 | COBS(payload) | 0x00
// Reference: S. Cheshire and M. Baker, "Consistent Overhead Byte Stuffing"

namespace COBS
{
    // Encode src[0..len-1] into dst. dst must have room for len + ceil(len/254) + 1 bytes.
    // Returns number of bytes written to dst (does NOT include framing delimiters).
    size_t encode(const uint8_t *src, size_t len, uint8_t *dst);

    // Decode a COBS block (between delimiters) from src[0..len-1] into dst.
    // Returns number of decoded bytes, or 0 on error.
    size_t decode(const uint8_t *src, size_t len, uint8_t *dst);
};
