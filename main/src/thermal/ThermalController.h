#pragma once
#include <PinConfig.h>
#include "Arduino.h"
#include "cJSON.h"

/*
 * ThermalController — NTC heat-sink monitoring for the illumination board.
 *
 * Hardware (illumination board rev. H, sheet "thermistors-and-buffer"):
 *
 *     +3V3 ──[ NTC 10k ]──┬──> LM324 unity-gain buffer ──[10k]──> ESP32 ADC pin
 *                         │
 *                       [ 1k ]
 *                         │
 *                        GND
 *
 * The NTC is the *upper* leg, so the node voltage RISES with temperature.
 * Thermistor: NCP15XH103F03RC, 10 kOhm @ 25 °C, B(25/80) = 3380 K.
 *
 * Four of these sit on the heat sink (temperature-sensor_1..4). Only part of
 * the LED matrix may be lit, so the sensors do not track each other — both the
 * mean (the control input) and the max are exposed.
 *
 * ADC notes:
 *  - All four pins are on ADC1, which is always usable on the ESP32-S3.
 *  - 6 dB attenuation gives an effective 0..1600 mV range with ±10 mV error,
 *    which is the range the divider actually uses (0.25 V @ 20 °C,
 *    1.45 V @ 90 °C). This is the attenuation the schematic asks for.
 *  - analogReadMilliVolts() applies the per-chip eFuse calibration, so it is
 *    used instead of scaling raw counts by 3.3/4095 (that naive conversion
 *    reads ~8-10 % low on the ESP32-S3).
 *  - Single reads scatter by roughly ±20 counts, so every reported value is a
 *    boxcar average over THERMAL_OVERSAMPLE sweeps spread across
 *    THERMAL_SAMPLE_PERIOD_MS each — long enough to average out mains ripple.
 *
 * Boards without the thermistors fitted keep thermal_PIN_* at `disabled` and
 * the module stays inert. On a board that has them, monitoring can still be
 * switched off at runtime with {"task":"/temp_act","enabled":0} (persisted).
 */

#define THERMAL_NUM_SENSORS 4

// One sweep of all four channels every 20 ms, averaged over 32 sweeps →
// a 640 ms window, which spans many 50/60 Hz mains periods.
#define THERMAL_SAMPLE_PERIOD_MS 20
#define THERMAL_OVERSAMPLE 32

namespace ThermalController
{
    enum State
    {
        STATE_UNKNOWN = 0, // no complete measurement window yet
        STATE_OK,
        STATE_WARN,     // above warnC — announced, power should be rolled back
        STATE_CRITICAL, // above criticalC — LED power is cut
        STATE_FAULT     // no sensor delivers a plausible reading
    };

    struct SensorReading
    {
        int8_t pin;
        uint16_t milliVolts; // averaged, eFuse-calibrated
        uint16_t raw;        // averaged raw counts (for comparison with bench measurements)
        float degC;          // NAN when faulted
        bool fault;
        bool saturated;
    };

    void setup();
    void loop();

    int act(cJSON *ob);
    cJSON *get(cJSON *ob);

    // --- consumers (LedController / FanController) ---

    // Mean over the non-faulted sensors; NAN when nothing is measurable.
    float getMeanC();
    // Hottest non-faulted sensor; NAN when nothing is measurable.
    float getMaxC();
    // True while the LEDs must stay off (over-temperature latch is set).
    // Always false when monitoring is disabled, so callers can use it
    // unconditionally.
    bool isLedCutActive();
    bool isEnabled();
    State getState();
}
