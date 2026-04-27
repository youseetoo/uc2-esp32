#pragma once
#include <PinConfig.h>
#include <Adafruit_NeoPixel.h>
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
namespace SignalController
{
    enum class SignalStatus : uint8_t
    {
        OFF = 0,
        INITIALIZING,        // white <-> black, 1s linger
        INIT_FAILED,         // dark red, slow
        IDLE,                // green pulse, no client
        CLIENT_ACTIVE,       // magenta/pink, 1s
        JOB_RUNNING,         // amber, 1s
        JOB_FAILED,          // red <-> black, 1s
        JOB_PARTLY_FAILED,   // red <-> green, 3s
        JOB_COMPLETE,        // green <-> brown, 3s
        RASPI_BUSY,          // green/green/magenta, 1s
        RASPI_SHUTDOWN,      // green/white/black, 1s
        ON,                  // solid white
        CUSTOM,
        UNKNOWN
    };

    struct ColorStop
    {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint16_t lingerMs = 1000; // time fully on this color
        bool active = true;        // false => skip
    };

    struct SignalPattern
    {
        ColorStop stops[3];
        uint16_t fadeMs = 500;     // fade duration between stops
    };

    void setup();
    void loop();

    // Apply a named status (one of the values in SignalStatus mapped from
    // strings such as "idle", "error", "busy", "off", ...).
    void setStatus(SignalStatus s);
    bool setStatusByName(const char *name);

    // Apply a fully custom 1..3 color pattern.
    void setCustom(const SignalPattern &p);

    // Global brightness 0..255.
    void setBrightness(uint8_t b);

    int act(cJSON *root);
    cJSON *get(cJSON *root);
}
