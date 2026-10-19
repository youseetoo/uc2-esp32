#pragma once
// T4.1: SignalController converted from a C++ namespace to a pure-C module.
// The Adafruit_NeoPixel C++ class dependency is hidden behind the
// signal_neopixel_c.h shim (implemented in signal_neopixel_c.cpp).
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"

// SignalController
// ---------------------------------------------------------------------------
// Drives the on-board status NeoPixel(s) on the FRAME (CAN HAT master) and
// signals state via a small set of named patterns following the brand guide.
// Independent from LedController / /ledarr_act which targets the (possibly
// remote, multi-pixel) light array. Patterns alternate between up to three
// colors with per-color "linger" durations and a fixed 500 ms fade.
//
// Endpoint: /signal_act
//   { "task": "/signal_act", "qid": 1, "signal": { "status": "idle" } }
//   { "task": "/signal_act", "qid": 1, "signal": { "status": "off" } }
//   { "task": "/signal_act", "qid": 1, "signal": { "status": "custom",
//        "primary":   { "color": "#85b918", "duration": 1 },
//        "secondary": { "color": "#FFFFFF", "duration": 1 },
//        "tertiary":  { "color": "#000000", "duration": 1 },
//        "brightness": 128 } }
//   { "task": "/signal_act", "qid": 1, "signal": { "brightness": 64 } }
// Endpoint: /signal_get   -> returns active status name + brightness

#ifdef __cplusplus
extern "C" {
#endif

// Former `enum class SignalStatus : uint8_t`.
typedef enum SignalStatus {
    SIGNAL_OFF = 0,
    SIGNAL_INITIALIZING,        // white <-> black, 1s linger
    SIGNAL_INIT_FAILED,         // dark red, slow
    SIGNAL_IDLE,                // green pulse, no client
    SIGNAL_CLIENT_ACTIVE,       // magenta/pink, 1s
    SIGNAL_JOB_RUNNING,         // amber, 1s
    SIGNAL_JOB_FAILED,          // red <-> black, 1s
    SIGNAL_JOB_PARTLY_FAILED,   // red <-> green, 3s
    SIGNAL_JOB_COMPLETE,        // green <-> brown, 3s
    SIGNAL_RASPI_BUSY,          // green/green/magenta, 1s
    SIGNAL_RASPI_SHUTDOWN,      // green/white/black, 1s
    SIGNAL_ON,                  // solid white
    SIGNAL_CUSTOM,
    SIGNAL_UNKNOWN
} SignalStatus;

typedef struct SignalColorStop {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint16_t lingerMs; // time fully on this color
    bool active;       // false => skip
} SignalColorStop;

typedef struct SignalPattern {
    SignalColorStop stops[3];
    uint16_t fadeMs; // fade duration between stops
} SignalPattern;

void signal_setup(void);
void signal_loop(void);

// Apply a named status (one of the values in SignalStatus mapped from
// strings such as "idle", "error", "busy", "off", ...).
void signal_set_status(SignalStatus s);
bool signal_set_status_by_name(const char *name);

// Apply a fully custom 1..3 color pattern.
void signal_set_custom(const SignalPattern *p);

// Global brightness 0..255.
void signal_set_brightness(uint8_t b);

int signal_act(cJSON *root);
cJSON *signal_get(cJSON *root);

#ifdef __cplusplus
}
#endif
