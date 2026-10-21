// FanController (Phase 4: pure C)
#include "FanController.h"
#include <uc2_hal/uc2_wire_c.h>
#include <uc2_hal/uc2_gpio.h>
#include <uc2_hal/uc2_time.h>
#include <uc2_hal/uc2_log.h>
#include <esp_attr.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <strings.h>
#include <math.h>
#include "cJsonTool.h"
#include "Tmp102Controller.h"

static const char *TAG = "FanController";

// ---- State ----
static bool     initialized   = false;
static fan_mode_t mode        = FAN_MODE_AUTO;
static uint8_t  current_wiper = FAN_WIPER_QUIET;
static uint8_t  manual_wiper  = FAN_WIPER_QUIET;
static bool     kick_enabled  = true;
static bool     stalled       = false;
static float    current_rpm   = 0.0f;
static uint32_t last_curve_ms = 0;
static uint32_t stall_start_ms = 0; // when we first noticed RPM=0 while commanded ON
static uint32_t kick_end_ms    = 0; // !=0 -> currently mid-kick, drop back at this time
static uint8_t  pre_kick_wiper = FAN_WIPER_QUIET;
static float    last_temp_applied_c = NAN; // temp last used by curve (for hysteresis)

// ---- Tacho ----
static volatile uint32_t tacho_pulses = 0;
static uint32_t last_tacho_sample_ms    = 0;

// ---- Curve ----
static fan_curve_point_t curve[FAN_MAX_CURVE_POINTS];
static uint8_t    curve_points = 0;

// NVS namespace (was uc2::Preferences "fan").
#define FAN_NVS_NS "fan"

// Default curve (ascending temperature thresholds):
//   < 35 °C  -> quiet
//   < 45 °C  -> low      (wiper 0x50 = 80)
//   < 55 °C  -> medium   (wiper 0x30 = 48)
//   < 65 °C  -> high     (wiper 0x10 = 16)
//   >= 65 °C -> full     (wiper 0x00 = 0)
static void load_default_curve(void)
{
    curve[0].threshold_c = 35.0f; curve[0].wiper = FAN_WIPER_QUIET;
    curve[1].threshold_c = 45.0f; curve[1].wiper = 0x50;
    curve[2].threshold_c = 55.0f; curve[2].wiper = 0x30;
    curve[3].threshold_c = 65.0f; curve[3].wiper = 0x10;
    curve[4].threshold_c = 200.0f; curve[4].wiper = FAN_WIPER_FULL; // anything above 65 -> full speed
    curve_points = 5;
}

// ===== Tacho ISR =====
// Pulled into IRAM so the ISR can run while flash is busy.
static void IRAM_ATTR on_tacho_edge(void)
{
    tacho_pulses = tacho_pulses + 1;
}

// ===== MCP4017 driver =====
// The MCP4017 is a single-byte device — every write is "set the wiper".
// No register address. 7-bit value, top bit ignored. Read returns the
// currently-stored wiper byte.
bool fan_set_wiper(uint8_t value)
{
    if (!initialized) return false;
    if (value > 0x7F) value = 0x7F;
    wire_begin_transmission(FAN_MCP4017_ADDR);
    wire_write(value);
    uint8_t err = wire_end_transmission(true);
    if (err != 0)
    {
        log_w("MCP4017: write %u failed (err=%u)", value, err);
        return false;
    }
    current_wiper = value;
    return true;
}

static uint8_t probe_wiper_from_chip(void)
{
    // requestFrom returns a single byte from the wiper register.
    if (wire_request_from(FAN_MCP4017_ADDR, 1) != 1)
    {
        log_w("MCP4017: probe read failed");
        return FAN_WIPER_POR;
    }
    return (uint8_t)(wire_read() & 0x7F);
}

uint8_t fan_get_wiper(void)  { return current_wiper; }
float   fan_get_rpm(void)    { return current_rpm; }
bool    fan_is_stalled(void) { return stalled; }

// ===== Curve evaluation with hysteresis =====
static uint8_t evaluate_curve(float temp_c)
{
    if (isnan(temp_c) || curve_points == 0) {
        return FAN_WIPER_QUIET;
    }

    // Choose the highest-temp breakpoint whose threshold is <= temp_c.
    // If none, pick the lowest-temp band (= quietest).
    uint8_t chosen = curve[0].wiper;
    for (uint8_t i = 0; i < curve_points; ++i) {
        if (temp_c >= curve[i].threshold_c) {
            // Use the *next* point's wiper if available — the band is
            // (curve[i].thresholdC, curve[i+1].thresholdC] so we ramp up
            // as we cross each threshold.
            if (i + 1 < curve_points) chosen = curve[i + 1].wiper;
            else                      chosen = curve[i].wiper;
        }
    }

    // Apply hysteresis: only allow the wiper to *quieten* (higher value)
    // if the temperature has dropped CURVE_HYSTERESIS below the current
    // band's threshold. This requires us to know which band we're in.
    if (!isnan(last_temp_applied_c) && chosen > current_wiper) {
        // Would quieten — only accept if temp fell more than HYSTERESIS
        // below the threshold that put us in the current (louder) band.
        if (temp_c > (last_temp_applied_c - FAN_CURVE_HYSTERESIS)) {
            return current_wiper; // hold
        }
    }
    return chosen;
}

// ===== Tacho sampling =====
static void sample_tacho(uint32_t now)
{
    uint32_t dt = now - last_tacho_sample_ms;
    if (dt < FAN_TACHO_WINDOW_MS) return;

    // Atomically snapshot + clear the ISR counter.
    noInterrupts();
    uint32_t pulses = tacho_pulses;
    tacho_pulses = 0;
    interrupts();

    // RPM = (pulses / pulses_per_rev) * (60_000 ms/min / dt_ms)
    current_rpm = ((float)pulses / (float)FAN_PULSES_PER_REV)
                  * (60000.0f / (float)dt);
    last_tacho_sample_ms = now;
}

// ===== NVS persistence (was uc2::Preferences fPrefs) =====
static void persist_mode(void)
{
    nvs_handle_t h = 0;
    if (nvs_open(FAN_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i32(h, "mode", (int32_t)mode);
    nvs_set_i32(h, "mwip", (int32_t)manual_wiper);
    nvs_set_i32(h, "kick", kick_enabled ? 1 : 0);
    nvs_commit(h);
    nvs_close(h);
}

static void persist_curve(void)
{
    fan_curve_point_t blob[FAN_MAX_CURVE_POINTS] = {0};
    for (uint8_t i = 0; i < curve_points && i < FAN_MAX_CURVE_POINTS; ++i) {
        blob[i] = curve[i];
    }
    // Mark unused slots with an obviously-invalid sentinel so the loader
    // knows where to stop.
    for (uint8_t i = curve_points; i < FAN_MAX_CURVE_POINTS; ++i) {
        blob[i].wiper       = 0xFF;
        blob[i].threshold_c = 0.0f;
    }
    nvs_handle_t h = 0;
    if (nvs_open(FAN_NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "curve", blob, sizeof(blob));
    nvs_commit(h);
    nvs_close(h);
}

// ===== Lifecycle =====
void fan_setup(void)
{
    log_i("FanController setup begin");

    // wire_begin() already done by tmp102_setup() / i2c_master.
    // Probe the MCP4017 — at POR it should return 0x3F.
    uint8_t probe = probe_wiper_from_chip();
    log_i("MCP4017 probe @0x%02X = 0x%02X (expected 0x3F at POR)",
          FAN_MCP4017_ADDR, probe);
    current_wiper = probe;

    // Tacho input — open-drain on the fan side, internal pull-up keeps the
    // line HIGH when idle, falling edge = rotation pulse.
    if (pinConfig.FAN_TACHO_PIN >= 0) {
        pinMode(pinConfig.FAN_TACHO_PIN, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(pinConfig.FAN_TACHO_PIN),
                        on_tacho_edge, FALLING);
        log_i("Tacho ISR attached on GPIO %d", pinConfig.FAN_TACHO_PIN);
    } else {
        log_w("FAN_TACHO_PIN disabled — RPM/stall detection unavailable");
    }

    // Load persisted mode / curve / kick flag from NVS.
    {
        nvs_handle_t h = 0;
        if (nvs_open(FAN_NVS_NS, NVS_READONLY, &h) == ESP_OK)
        {
            int32_t v = 0;
            if (nvs_get_i32(h, "mode", &v) == ESP_OK && v >= FAN_MODE_AUTO && v <= FAN_MODE_OFF)
                mode = (fan_mode_t)v;
            if (nvs_get_i32(h, "mwip", &v) == ESP_OK)
                manual_wiper = (uint8_t)v;
            if (nvs_get_i32(h, "kick", &v) == ESP_OK)
                kick_enabled = (v != 0);

            fan_curve_point_t tmp[FAN_MAX_CURVE_POINTS];
            size_t blob_size = sizeof(tmp);
            if (nvs_get_blob(h, "curve", tmp, &blob_size) == ESP_OK &&
                blob_size == sizeof(fan_curve_point_t) * FAN_MAX_CURVE_POINTS) {
                // Trust persisted curve if it has at least one valid breakpoint.
                uint8_t n = 0;
                for (; n < FAN_MAX_CURVE_POINTS; ++n) {
                    if (tmp[n].wiper > 0x7F) break;
                    curve[n] = tmp[n];
                }
                if (n > 0) curve_points = n;
                else       load_default_curve();
            }
            nvs_close(h);
        }
        if (curve_points == 0)
            load_default_curve();
    }

    initialized = true;

    // Verify the chip is present before proceeding. If the quiet write
    // fails we disable the module so loop() never polls a missing device.
    if (!fan_set_wiper(FAN_WIPER_QUIET)) {
        log_w("MCP4017 @ 0x%02X not present — FanController disabled", FAN_MCP4017_ADDR);
        initialized = false;
        return;
    }

    // Startup blast — drive the fan full for a moment to confirm it
    // unsticks. We do this unconditionally so the user gets feedback
    // that the new control path works.
    log_i("Startup blast: wiper=0x00 for %u ms", (unsigned)FAN_STARTUP_BLAST_MS);
    fan_set_wiper(FAN_WIPER_FULL);
    delay(FAN_STARTUP_BLAST_MS);

    // Drop into the configured idle state. The control loop will reach
    // for the curve as soon as it ticks.
    if (mode == FAN_MODE_MANUAL)  fan_set_wiper(manual_wiper);
    else if (mode == FAN_MODE_OFF) fan_set_wiper(FAN_WIPER_QUIET);
    else                          fan_set_wiper(FAN_WIPER_QUIET); // auto starts quiet

    last_curve_ms       = millis();
    last_tacho_sample_ms = millis();
}

void fan_loop(void)
{
    if (!initialized) return;
    uint32_t now = millis();

    sample_tacho(now);

    // End an in-progress kick.
    if (kick_end_ms != 0 && (int32_t)(now - kick_end_ms) >= 0) {
        kick_end_ms = 0;
        fan_set_wiper(pre_kick_wiper);
        log_i("Kick complete -> wiper=0x%02X", pre_kick_wiper);
    }

    // Stall detection: if we commanded the fan ON (wiper < QUIET) but the
    // tacho has been silent for a while, flag it and (optionally) kick.
    bool commanded_on = (current_wiper < FAN_WIPER_QUIET);
    if (commanded_on && current_rpm < 1.0f && pinConfig.FAN_TACHO_PIN >= 0) {
        if (stall_start_ms == 0) stall_start_ms = now;
        if ((now - stall_start_ms) >= FAN_KICK_STALL_GRACE_MS) {
            stalled = true;
            if (kick_enabled && kick_end_ms == 0) {
                pre_kick_wiper = current_wiper;
                fan_set_wiper(FAN_WIPER_FULL);
                kick_end_ms = now + FAN_KICK_DURATION_MS;
                log_i("Stall detected — kicking from 0x%02X to 0x00",
                      pre_kick_wiper);
                // Reset stallStartMs so we don't immediately re-trigger.
                stall_start_ms = now;
            }
        }
    } else {
        stall_start_ms = 0;
        stalled        = false;
    }

    // Curve / mode application (only when not mid-kick).
    if (kick_end_ms == 0 && (now - last_curve_ms) >= FAN_CURVE_TICK_MS) {
        last_curve_ms = now;
        uint8_t target_wiper = current_wiper;

        switch (mode) {
            case FAN_MODE_AUTO: {
                float temp_c = tmp102_get_max_ambient_c();
                target_wiper = evaluate_curve(temp_c);
                last_temp_applied_c = temp_c;
                break;
            }
            case FAN_MODE_MANUAL:
                target_wiper = manual_wiper;
                break;
            case FAN_MODE_OFF:
                target_wiper = FAN_WIPER_QUIET;
                break;
        }

        if (target_wiper != current_wiper) {
            fan_set_wiper(target_wiper);
        }
    }
}

// ===== JSON I/O =====
static const char *mode_name(fan_mode_t m)
{
    switch (m) {
        case FAN_MODE_AUTO:   return "auto";
        case FAN_MODE_MANUAL: return "manual";
        case FAN_MODE_OFF:    return "off";
    }
    return "?";
}

int fan_act(cJSON *root)
{
    int qid = cjson_tool_get_int(root, "qid", 0);
    cJSON *fan = cJSON_GetObjectItem(root, "fan");
    if (!fan || !cJSON_IsObject(fan)) {
        log_w("/fan_act: missing 'fan' object");
        return qid;
    }

    bool mode_changed  = false;
    bool curve_changed = false;

    cJSON *jmode = cJSON_GetObjectItem(fan, "mode");
    if (jmode && cJSON_IsString(jmode)) {
        const char *s = jmode->valuestring;
        if      (!strcasecmp(s, "auto"))   { mode = FAN_MODE_AUTO;   mode_changed = true; }
        else if (!strcasecmp(s, "manual")) { mode = FAN_MODE_MANUAL; mode_changed = true; }
        else if (!strcasecmp(s, "off"))    { mode = FAN_MODE_OFF;    mode_changed = true; }
        else log_w("/fan_act: unknown mode '%s'", s);
    }

    cJSON *jwiper = cJSON_GetObjectItem(fan, "wiper");
    if (jwiper && cJSON_IsNumber(jwiper)) {
        int w = jwiper->valueint;
        if (w < 0) w = 0;
        if (w > 0x7F) w = 0x7F;
        manual_wiper = (uint8_t)w;
        // If the caller set a wiper without specifying a mode, assume manual.
        if (!jmode) { mode = FAN_MODE_MANUAL; mode_changed = true; }
        mode_changed = true;
    }

    cJSON *jkick = cJSON_GetObjectItem(fan, "kick");
    if (jkick && (cJSON_IsBool(jkick) || cJSON_IsNumber(jkick))) {
        kick_enabled = cJSON_IsTrue(jkick) ||
                       (cJSON_IsNumber(jkick) && jkick->valueint != 0);
        mode_changed = true;
    }

    cJSON *jcurve = cJSON_GetObjectItem(fan, "curve");
    if (jcurve && cJSON_IsArray(jcurve)) {
        uint8_t n = 0;
        fan_curve_point_t tmp[FAN_MAX_CURVE_POINTS];
        int sz = cJSON_GetArraySize(jcurve);
        for (int i = 0; i < sz && n < FAN_MAX_CURVE_POINTS; ++i) {
            cJSON *pair = cJSON_GetArrayItem(jcurve, i);
            if (!pair || !cJSON_IsArray(pair) || cJSON_GetArraySize(pair) < 2) continue;
            cJSON *jt = cJSON_GetArrayItem(pair, 0);
            cJSON *jw = cJSON_GetArrayItem(pair, 1);
            if (!jt || !jw || !cJSON_IsNumber(jt) || !cJSON_IsNumber(jw)) continue;
            int w = jw->valueint;
            if (w < 0) w = 0;
            if (w > 0x7F) w = 0x7F;
            tmp[n].threshold_c = (float)jt->valuedouble;
            tmp[n].wiper       = (uint8_t)w;
            ++n;
        }
        if (n > 0) {
            curve_points = n;
            for (uint8_t i = 0; i < n; ++i) curve[i] = tmp[i];
            curve_changed = true;
        } else {
            log_w("/fan_act: empty/invalid curve, keeping current");
        }
    }

    // Apply immediately so the caller sees the change without waiting for
    // the next CURVE_TICK_MS.
    if (mode == FAN_MODE_MANUAL)    fan_set_wiper(manual_wiper);
    else if (mode == FAN_MODE_OFF)  fan_set_wiper(FAN_WIPER_QUIET);
    // FAN_MODE_AUTO: let the loop pick up next tick.

    if (mode_changed)  persist_mode();
    if (curve_changed) persist_curve();

    log_i("/fan_act: mode=%s wiper=0x%02X manual=0x%02X kick=%d curvePts=%u",
          mode_name(mode), current_wiper, manual_wiper, kick_enabled, curve_points);
    return qid;
}

cJSON *fan_get(cJSON *root)
{
    (void)root;
    cJSON *ret = cJSON_CreateObject();
    cJSON *fan = cJSON_CreateObject();

    cJSON_AddStringToObject(fan, "mode", mode_name(mode));
    cjson_tool_set_int(fan, "wiper",   current_wiper);
    cjson_tool_set_int(fan, "manual",  manual_wiper);
    cjson_tool_set_float(fan, "rpm",   current_rpm);
    cjson_tool_set_int(fan, "stalled", stalled ? 1 : 0);
    cjson_tool_set_int(fan, "kick",    kick_enabled ? 1 : 0);
    cjson_tool_set_float(fan, "tempC", tmp102_get_max_ambient_c());

    cJSON *jcurve = cJSON_CreateArray();
    for (uint8_t i = 0; i < curve_points; ++i) {
        cJSON *pair = cJSON_CreateArray();
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(curve[i].threshold_c));
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(curve[i].wiper));
        cJSON_AddItemToArray(jcurve, pair);
    }
    cJSON_AddItemToObject(fan, "curve", jcurve);

    cJSON_AddItemToObject(ret, "fan", fan);
    return ret;
}
