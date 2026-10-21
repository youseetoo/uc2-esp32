#pragma once
// FanController (Phase 4: pure C)
// ---------------------------------------------------------------------------
// Heat-controlled case-fan management for the CAN-HAT v2.
//
// Hardware:
//   * MCP4017T-503E digital rheostat at I2C 0x2F sets the fan voltage on the
//     panelboard. Wiper 0x00 = minimum resistance = ~12V (full speed).
//     Wiper 0x7F = maximum resistance = lowest fan voltage (off / quiet).
//     POR default = 0x3F (mid).
//   * Open-drain tachometer on FAN_TACHO_PIN (GPIO33). The fan pulls the
//     line LOW twice per revolution (Noctua convention).
//   * Heat sources: TMP102 PCB sensor (0x4A), TMP102 air sensor (0x4B),
//     ESP internal sensor (fallback).
//
// Modes (selectable via /fan_act):
//   * FAN_MODE_AUTO   — apply the temperature curve every CURVE_TICK_MS,
//                       with hysteresis to avoid oscillation.
//   * FAN_MODE_MANUAL — hold a wiper value chosen by the caller.
//   * FAN_MODE_OFF    — hold wiper at 0x7F (lowest voltage).
//
// Optional behaviours:
//   * STARTUP BLAST: on setup(), drive the wiper to 0x00 for STARTUP_BLAST_MS
//     so the fan reliably gets going, then drop into the configured mode.
//   * KICK-START: when the curve wants the fan on but the tacho still reads
//     zero RPM after KICK_STALL_GRACE_MS, briefly snap wiper to 0x00 for
//     KICK_DURATION_MS to break stiction.
//
// Endpoints:
//   /fan_act:
//     { "task":"/fan_act", "fan":{"mode":"auto"} }
//     { "task":"/fan_act", "fan":{"mode":"manual", "wiper":48} }
//     { "task":"/fan_act", "fan":{"mode":"off"} }
//     { "task":"/fan_act", "fan":{"kick":1} }                  // toggle kick-start
//     { "task":"/fan_act", "fan":{"curve":[[35,127],[45,80],[55,48],[65,16],[75,0]]} }
//
//   /fan_get -> reports current mode, wiper, last RPM, stall flag, curve.
#include <stdbool.h>
#include <stdint.h>
#include <PinConfig.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- Hardware tunables ----
#define FAN_MCP4017_ADDR  0x2F  // mirrored from PinConfigDefault
#define FAN_WIPER_FULL    0x00  // max voltage / full speed
#define FAN_WIPER_QUIET   0x7F  // min voltage / off
#define FAN_WIPER_POR     0x3F  // power-on default we expect

// ---- Tacho ----
#define FAN_PULSES_PER_REV   2     // Noctua-style 4-pin convention
#define FAN_TACHO_WINDOW_MS  1000  // RPM update cadence

// ---- Control loop ----
#define FAN_CURVE_TICK_MS       100
#define FAN_CURVE_HYSTERESIS    2.0f  // °C
#define FAN_KICK_STALL_GRACE_MS 2500
#define FAN_KICK_DURATION_MS    500
#define FAN_STARTUP_BLAST_MS    1500

// ---- Curve format ----
// A monotonic-ascending list of (temp_threshold_C, wiper) breakpoints.
// The control loop picks the lowest-temp breakpoint whose threshold is
// BELOW the current temperature, with HYSTERESIS to avoid bounce.
#define FAN_MAX_CURVE_POINTS 8

typedef struct {
    float   threshold_c;
    uint8_t wiper;
} fan_curve_point_t;

typedef enum {
    FAN_MODE_AUTO   = 0,
    FAN_MODE_MANUAL = 1,
    FAN_MODE_OFF    = 2,
} fan_mode_t;

void   fan_setup(void);
void   fan_loop(void);

// Low-level access (used by other modules and unit tests).
bool   fan_set_wiper(uint8_t value);
uint8_t fan_get_wiper(void);
float  fan_get_rpm(void);
bool   fan_is_stalled(void);

int    fan_act(cJSON *root);
cJSON *fan_get(cJSON *root);

#ifdef __cplusplus
}  // extern "C"
#endif
