#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "PinConfigDefault.h"
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

#ifdef __cplusplus
extern "C" {
#endif

void buzzer_setup(void);
void buzzer_loop(void);

// Start a tone at `freq` (Hz). 0 stops the buzzer.
// If durationMs > 0, the tone is automatically stopped from buzzer_loop().
void buzzer_tone(uint32_t freq, uint32_t durationMs);
void buzzer_stop(void);

// Queue a sequence of `count` beeps of `freq`/`durationMs` separated by
// `gapMs` of silence. Non-blocking; played from buzzer_loop().
void buzzer_beep(uint8_t count, uint32_t freq, uint32_t durationMs, uint32_t gapMs);

// Convenience presets ("ok", "error", "warn", "click", "shutdown").
bool buzzer_preset(const char *name);

int buzzer_act(cJSON *root);

#ifdef __cplusplus
}
#endif
