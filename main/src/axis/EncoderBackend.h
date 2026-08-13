#pragma once
#include <Arduino.h>

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
// Direction inversion is applied in EXACTLY ONE place (getCount, from config).
// ============================================================================

class EncoderBackend
{
public:
    // PCNT unit reserved for FastAccelStepper's step generator. The encoder
    // must never be configured onto this unit.
    static constexpr int FAS_PCNT_UNIT = 0;

    EncoderBackend() = default;

    // Configure the encoder on the given A/B pins.
    //   invert       : negate the count once, here, from pin config.
    //   pcntUnit     : explicit PCNT unit index (must differ from FAS_PCNT_UNIT).
    //   glitchFilter : ESP32Encoder glitch filter value (0 = off).
    // Returns true if attached to real hardware. Safe (returns false) when the
    // pins are disabled or the counter peripheral is not compiled in.
    bool begin(int8_t pinA, int8_t pinB, bool invert, int pcntUnit = 1,
               uint16_t glitchFilter = 1);

    // Signed encoder count with direction inversion already applied. 0 if absent.
    int64_t getCount();

    // Reset the hardware counter to zero.
    void zero();

    // True once begin() has attached to a real encoder.
    bool isPresent() const { return present; }

    // Change the glitch filter at runtime (0 = off, higher = more filtering).
    void setGlitchFilter(uint16_t value);

    int getUnit() const { return unitIndex; }

private:
    bool     present    = false;
    bool     invertDir  = false;
    int      unitIndex  = -1;
    int8_t   aPin       = -1;
    int8_t   bPin       = -1;
    void    *encoderImpl = nullptr; // ESP32Encoder* (opaque to keep header light)
};
