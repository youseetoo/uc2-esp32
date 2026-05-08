#pragma once
#include <PinConfig.h>
#include "cJSON.h"

// BuzzerController
// ---------------------------------------------------------------------------
// Drives the on-board piezo buzzer (HAT v2 GPIO25) with simple beeps and
// tones using the ESP32 LEDC PWM peripheral. Independent from any other
// audio/DAC code.
//
// Endpoint: /buzzer_act
//   { "task": "/buzzer_act", "qid": 1, "buzzer": { "freq": 2000, "duration": 200 } }
//   { "task": "/buzzer_act", "qid": 1, "buzzer": { "beeps": 3, "freq": 4000, "duration": 80, "gap": 80 } }
//   { "task": "/buzzer_act", "qid": 1, "buzzer": { "preset": "error" } }
//   { "task": "/buzzer_act", "qid": 1, "buzzer": { "stop": true } }
namespace BuzzerController
{
    void setup();
    void loop();

    // Start a tone at `freq` (Hz). 0 stops the buzzer.
    // If durationMs > 0, the tone is automatically stopped from loop().
    void tone(uint32_t freq, uint32_t durationMs = 0);
    void stop();

    // Queue a sequence of `count` beeps of `freq`/`durationMs` separated by
    // `gapMs` of silence. Non-blocking; played from loop().
    void beep(uint8_t count, uint32_t freq, uint32_t durationMs, uint32_t gapMs);

    // Convenience presets ("ok", "error", "warn", "click", "shutdown").
    bool preset(const char *name);

    int act(cJSON *root);
}
