#pragma once
// T4.1: pure-C API (was a C++ class). The ESP32Encoder (C++ library)
// integration lives in EncoderBackend.cpp, which implements these extern "C"
// functions over the C struct below.
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// EncoderBackend — clean A/B-quadrature-only reader (design v2, WP2).
//
// Replaces main/src/encoder/PCNTEncoderController.cpp. There is NO SSI/PWM/
// index/magnet-health hardware wired (constraint #1), so this is A/B only —
// no absolute-mode code paths.
//
// PCNT coordination (constraint #4): FastAccelStepper's MCPWM backend hard-maps
// stepper queue 0 -> PCNT_UNIT_0. ESP32Encoder::attach() otherwise grabs the
// first free PCNT slot, which would collide with FAS. This backend pins an
// EXPLICIT unit index (config-driven, default 1) by reserving the lower slots,
// and logs an error if the configured unit collides with the FAS unit.
//
// Direction inversion is applied in EXACTLY ONE place (get_count, from config).
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// PCNT unit reserved for FastAccelStepper's step generator. The encoder
// must never be configured onto this unit.
#define ENCODER_BACKEND_FAS_PCNT_UNIT 0

typedef struct EncoderBackend {
    bool     present;       // true once begin() attached to real hardware
    bool     invertDir;     // negate the count, applied in get_count only
    int      unitIndex;     // PCNT unit (-1 before begin)
    int8_t   aPin;
    int8_t   bPin;
    void    *encoderImpl;   // ESP32Encoder* (opaque to keep this header C)
} EncoderBackend;

// Zero the struct (C has no default constructor).
void encoder_backend_init(EncoderBackend *be);

// Configure the encoder on the given A/B pins.
//   invert       : negate the count once, here, from pin config.
//   pcntUnit     : explicit PCNT unit index (must differ from
//                  ENCODER_BACKEND_FAS_PCNT_UNIT).
//   glitchFilter : ESP32Encoder glitch filter value (0 = off).
// Returns true if attached to real hardware. Safe (returns false) when the
// pins are disabled or the counter peripheral is not compiled in.
bool encoder_backend_begin(EncoderBackend *be, int8_t pinA, int8_t pinB,
                           bool invert, int pcntUnit, uint16_t glitchFilter);

// Signed encoder count with direction inversion already applied. 0 if absent.
int64_t encoder_backend_get_count(const EncoderBackend *be);

// Reset the hardware counter to zero.
void encoder_backend_zero(EncoderBackend *be);

// Change the glitch filter at runtime (0 = off, higher = more filtering).
void encoder_backend_set_glitch_filter(EncoderBackend *be, uint16_t value);

#ifdef __cplusplus
}
#endif
