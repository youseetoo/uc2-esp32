// T4.1: ThermalController (pure C; was the C++ namespace version of T2.4).
// Preferences -> nvs_* direct ("thermal" namespace), analogRead* -> uc2_adc
// (eFuse-calibrated mV on S3), millis/log_* -> uc2_time/uc2_log.
// Serial output goes through the framed C path (serial_out_safe_serialize_json).
#include "ThermalController.h"
#include "PinConfigDefault.h"
#include "cJsonTool.h"
#include "uc2_serial_out.h"        // serial_out_safe_serialize_json
#include <uc2_hal/uc2_adc.h>       // analogRead*
#include <uc2_hal/uc2_time.h>      // millis()
#include <uc2_hal/uc2_log.h>       // log_i/w/e (forwards to TAG)
#include <nvs.h>
#include <math.h>
#include <stdio.h>

#ifdef LED_CONTROLLER
// C shim over LedController::turnOff() (C++ component); implemented as an
// extern "C" wrapper in LedController.cpp.
void led_c_turn_off(void);
#endif
#ifdef LASER_CONTROLLER
#include "LaserController.h"       // laser_set_val (pure C)
#endif

static const char *TAG = "Thermal";

// NVS namespace (was uc2::Preferences tPrefs "thermal").
#define THERMAL_NVS_NS "thermal"

// ---- configuration (NVS-backed) ----
static bool enabled = false;
static float warnC = 58.0f;
static float criticalC = 70.0f;
static float recoverC = 50.0f;

// ---- runtime state ----
static bool haveAnyPin = false;
static int state = THERMAL_STATE_UNKNOWN;
static int announcedState = THERMAL_STATE_UNKNOWN;
static bool ledCutActive = false;
// Set once any sensor has produced a plausible reading. Lets us tell
// "this board never had thermistors" from "a sensor just died".
static bool sawValidReading = false;
static float meanC = NAN;
static float maxC = NAN;
static thermal_reading_t readings[THERMAL_NUM_SENSORS];

// ---- sampling accumulators ----
static uint32_t accMv[THERMAL_NUM_SENSORS];
static uint16_t lastRaw[THERMAL_NUM_SENSORS];
static uint8_t sweepCount = 0;
static uint32_t lastSweepMs = 0;

// A divider node below this cannot be a real temperature: at -20 °C the
// node still sits around 43 mV, so anything lower means the NTC is open or
// the pin is floating (both read "ice cold", which is the dangerous
// direction — it has to be caught explicitly).
static const uint16_t THERMAL_MV_FAULT_FLOOR = 50;
// Averaged raw counts this close to full scale mean the input has run past
// the 6 dB range (≈1600 mV, reached just under 100 °C).
static const uint16_t THERMAL_RAW_SATURATED = 4000;

// ── NCP15XH103F03RC resistance/temperature table ──────────────────────
// Values quoted on the board schematic (Murata datasheet, "XH103").
// Interpolated log-linearly, i.e. linear in (1/T, ln R), which is the
// local-Beta form and stays within a few tenths of a degree between the
// tabulated points — noticeably better than one Beta constant across the
// whole span (a single B=3380 reads ~1.3 °C low at 90 °C, and reading low
// is exactly the wrong direction for a safety cut-off).
typedef struct {
    float degC;
    float ohm;
} NtcPoint;
static const NtcPoint NTC_TABLE[] = {
    {20.0f, 12100.0f},
    {25.0f, 10000.0f},
    {40.0f, 5800.0f},
    {50.0f, 4200.0f},
    {60.0f, 3000.0f},
    {80.0f, 1669.0f},
    {85.0f, 1452.0f},
    {90.0f, 1268.0f},
};
static const size_t NTC_TABLE_LEN = sizeof(NTC_TABLE) / sizeof(NTC_TABLE[0]);

// Interpolate/extrapolate between two table points in (1/T, ln R) space.
static float interpolateSegment(const NtcPoint *a, const NtcPoint *b, float rOhm)
{
    float invTa = 1.0f / (a->degC + 273.15f);
    float invTb = 1.0f / (b->degC + 273.15f);
    float lnRa = logf(a->ohm);
    float lnRb = logf(b->ohm);
    // ln R = lnRa + beta * (1/T - 1/Ta)
    float beta = (lnRb - lnRa) / (invTb - invTa);
    float invT = invTa + (logf(rOhm) - lnRa) / beta;
    if (invT <= 0.0f)
        return NAN;
    return (1.0f / invT) - 273.15f;
}

static float resistanceToCelsius(float rOhm)
{
    size_t i;
    if (!(rOhm > 0.0f) || isinf(rOhm))
        return NAN;
    // Table runs cold→hot, i.e. resistance high→low.
    if (rOhm >= NTC_TABLE[0].ohm) // colder than 20 °C — extrapolate down
        return interpolateSegment(&NTC_TABLE[0], &NTC_TABLE[1], rOhm);
    for (i = 1; i < NTC_TABLE_LEN; ++i)
    {
        if (rOhm >= NTC_TABLE[i].ohm)
            return interpolateSegment(&NTC_TABLE[i - 1], &NTC_TABLE[i], rOhm);
    }
    // hotter than 90 °C — extrapolate with the top segment
    return interpolateSegment(&NTC_TABLE[NTC_TABLE_LEN - 2],
                              &NTC_TABLE[NTC_TABLE_LEN - 1], rOhm);
}

// Node voltage → NTC resistance. Upper leg is the NTC, lower leg is fixed:
//   V = Vsupply * Rlow / (Rlow + Rntc)  ->  Rntc = Rlow * (Vsupply - V) / V
static float milliVoltsToResistance(uint16_t mv)
{
    float supply = (float)pinConfig.thermal_supply_mv;
    if (mv == 0 || (float)mv >= supply)
        return NAN;
    return (float)pinConfig.thermal_series_r_ohm * (supply - (float)mv) / (float)mv;
}

static int8_t pinForIndex(uint8_t i)
{
    switch (i)
    {
    case 0: return pinConfig.thermal_PIN_0;
    case 1: return pinConfig.thermal_PIN_1;
    case 2: return pinConfig.thermal_PIN_2;
    case 3: return pinConfig.thermal_PIN_3;
    }
    return disabled;
}

static const char *stateName(int s)
{
    switch (s)
    {
    case THERMAL_STATE_OK:       return "OK";
    case THERMAL_STATE_WARN:     return "WARN";
    case THERMAL_STATE_CRITICAL: return "CRITICAL";
    case THERMAL_STATE_FAULT:    return "FAULT";
    default:                     return "UNKNOWN";
    }
}

// ── unsolicited notifications ────────────────────────────────────────
// Pushed through the framed serial-out C path so they get the same "++\n...\n--\n"
// framing and the same thread-safe output queue as every other reply.
static void announce(const char *event, const char *detail)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return;
    cJSON *t = cJSON_CreateObject();
    if (t == NULL)
    {
        cJSON_Delete(root);
        return;
    }
    cJSON_AddStringToObject(t, "event", event);
    cJSON_AddStringToObject(t, "state", stateName(state));
    if (!isnan(meanC))
        cjson_tool_set_float(t, "meanC", meanC);
    if (!isnan(maxC))
        cjson_tool_set_float(t, "maxC", maxC);
    if (detail != NULL)
        cJSON_AddStringToObject(t, "detail", detail);
    cJSON_AddItemToObject(root, "temp", t);
    serial_out_safe_serialize_json(root); // frames with ++\n..\n--\n
    cJSON_Delete(root);
}

// ── power cut ────────────────────────────────────────────────────────
static void cutLedPower(void)
{
#ifdef LED_CONTROLLER
    led_c_turn_off();
#endif
#ifdef LASER_CONTROLLER
    // The high-power white LED is exposed as laser channel 0 on this board.
    laser_set_val(0, 0);
#endif
}

// ── persistence ──────────────────────────────────────────────────────
static void persist(void)
{
    nvs_handle_t h = 0;
    if (nvs_open(THERMAL_NVS_NS, NVS_READWRITE, &h) != ESP_OK)
        return;
    nvs_set_i32(h, "en", enabled ? 1 : 0);
    nvs_set_float(h, "warn", warnC);   // IDF 6.1: nvs_set_f32 -> nvs_set_float
    nvs_set_float(h, "crit", criticalC);
    nvs_set_float(h, "recov", recoverC);
    nvs_commit(h);
    nvs_close(h);
}

// ── lifecycle ────────────────────────────────────────────────────────
void thermal_setup(void)
{
    // gpio_config() logs a line at INFO for each channel configure; four
    // channels of periodic sampling would flood the 115200-baud link and
    // truncate the JSON replies sharing it. Nothing else needs gpio at INFO.
    esp_log_level_set("gpio", ESP_LOG_WARN);

    for (uint8_t i = 0; i < THERMAL_NUM_SENSORS; ++i)
    {
        readings[i].pin = pinForIndex(i);
        readings[i].milliVolts = 0;
        readings[i].raw = 0;
        readings[i].degC = NAN;
        readings[i].fault = true;
        readings[i].saturated = false;
        accMv[i] = 0;
        lastRaw[i] = 0;
        if (readings[i].pin >= 0)
        {
            haveAnyPin = true;
            // 6 dB → 0..1600 mV effective range, ±10 mV, as specified on
            // the schematic. The divider tops out at ~1.45 V @ 90 °C.
            analogSetPinAttenuation(readings[i].pin, ADC_6db);
        }
    }

    // Load configuration (NVS "thermal", defaults from the board pinconfig).
    enabled = pinConfig.thermal_enabled_default;
    warnC = pinConfig.thermal_warn_c;
    criticalC = pinConfig.thermal_critical_c;
    recoverC = pinConfig.thermal_recover_c;
    {
        nvs_handle_t h = 0;
        if (nvs_open(THERMAL_NVS_NS, NVS_READONLY, &h) == ESP_OK)
        {
            int32_t en = 0;
            float warn = 0.0f, crit = 0.0f, recov = 0.0f;
            if (nvs_get_i32(h, "en", &en) == ESP_OK)
                enabled = (en != 0);
            if (nvs_get_float(h, "warn", &warn) == ESP_OK)
                warnC = warn;
            if (nvs_get_float(h, "crit", &crit) == ESP_OK)
                criticalC = crit;
            if (nvs_get_float(h, "recov", &recov) == ESP_OK)
                recoverC = recov;
            nvs_close(h);
        }
    }

    if (!haveAnyPin)
    {
        // Board revision without thermistors — stay inert regardless of the
        // stored preference, so an old board cannot latch its LEDs off.
        enabled = false;
        log_i("No thermal_PIN_* configured — thermal monitoring disabled");
        return;
    }

    log_i("Thermal monitoring %s: pins %d/%d/%d/%d, warn %.1f °C, critical %.1f °C, recover %.1f °C",
          enabled ? "enabled" : "disabled",
          readings[0].pin, readings[1].pin, readings[2].pin, readings[3].pin,
          (double)warnC, (double)criticalC, (double)recoverC);

    lastSweepMs = millis();
}

// Turn one finished averaging window into per-sensor temperatures.
static void evaluateWindow(void)
{
    uint8_t validCount = 0;
    uint8_t faultCount = 0;
    float sum = 0.0f;
    float hottest = NAN;
    bool anySaturated = false;

    for (uint8_t i = 0; i < THERMAL_NUM_SENSORS; ++i)
    {
        thermal_reading_t *r = &readings[i];
        if (r->pin < 0)
        {
            r->fault = true;
            r->degC = NAN;
            continue;
        }

        r->milliVolts = (uint16_t)(accMv[i] / sweepCount);
        r->raw = lastRaw[i];
        r->saturated = (r->raw >= THERMAL_RAW_SATURATED);

        if (r->milliVolts < THERMAL_MV_FAULT_FLOOR)
        {
            // Open thermistor or floating pin — reads ice cold, which would
            // silently disable the protection. Treat as a fault instead.
            r->fault = true;
            r->degC = NAN;
            faultCount++;
            continue;
        }

        float rOhm = milliVoltsToResistance(r->milliVolts);
        float degC = resistanceToCelsius(rOhm);
        if (isnan(degC))
        {
            r->fault = true;
            r->degC = NAN;
            faultCount++;
            continue;
        }

        r->fault = false;
        r->degC = degC;
        if (r->saturated)
            anySaturated = true;
        sum += degC;
        if (isnan(hottest) || degC > hottest)
            hottest = degC;
        validCount++;
    }

    if (validCount > 0)
    {
        meanC = sum / (float)validCount;
        maxC = hottest;
        sawValidReading = true;
    }
    else
    {
        meanC = NAN;
        maxC = NAN;
    }

    // Nothing measurable and nothing ever was: this is almost certainly a
    // board revision without the thermistors fitted, running a rev.-H
    // config. Disable monitoring instead of latching the LEDs off — an
    // absent sensor is a configuration problem, not a thermal event.
    if (validCount == 0 && !sawValidReading)
    {
        state = THERMAL_STATE_FAULT;
        announce("fault",
                 "no thermistors detected - thermal monitoring disabled. "
                 "Fit rev. H sensors, or keep it off with "
                 "{\"task\":\"/temp_act\",\"enabled\":0}");
        announcedState = THERMAL_STATE_FAULT;
        enabled = false;
        ledCutActive = false;
        log_w("No plausible thermistor reading on any channel — thermal monitoring disabled");
        return;
    }

    // ── state machine ────────────────────────────────────────────────
    int next;
    const char *detail = NULL;

    if (validCount == 0)
    {
        // Sensors that worked before have stopped reading — an open NTC
        // reads ice cold, so failing safe is the only correct answer.
        next = THERMAL_STATE_FAULT;
        detail = "thermistor reading lost - check wiring, or disable "
                 "with {\"task\":\"/temp_act\",\"enabled\":0}";
    }
    else if (anySaturated)
    {
        // Past the top of the ADC range (~100 °C) — treat as critical.
        next = THERMAL_STATE_CRITICAL;
        detail = "sensor above ADC range";
    }
    else if (meanC >= criticalC)
    {
        next = THERMAL_STATE_CRITICAL;
    }
    else if (meanC >= warnC)
    {
        next = THERMAL_STATE_WARN;
    }
    else if (ledCutActive || state == THERMAL_STATE_WARN || state == THERMAL_STATE_CRITICAL || state == THERMAL_STATE_FAULT)
    {
        // Hysteresis: only fall back to OK once we are properly cool again,
        // so a reading hovering on the threshold cannot chatter.
        next = (meanC <= recoverC) ? THERMAL_STATE_OK : state;
    }
    else
    {
        next = THERMAL_STATE_OK;
    }

    state = next;

    // ── actions ──────────────────────────────────────────────────────
    if ((state == THERMAL_STATE_CRITICAL || state == THERMAL_STATE_FAULT) && !ledCutActive)
    {
        ledCutActive = true;
        cutLedPower();
        log_e("Over-temperature (mean %.1f °C, max %.1f °C) — LED power cut",
              (double)meanC, (double)maxC);
    }
    else if (ledCutActive && state == THERMAL_STATE_OK)
    {
        // Release the latch, but leave the LEDs off — the host has to send
        // a fresh command, so nothing lights up again behind the user's back.
        ledCutActive = false;
        log_i("Temperature recovered (mean %.1f °C) — LED commands accepted again",
              (double)meanC);
    }

    // ── announce state changes, unsolicited ──────────────────────────
    if (state != announcedState)
    {
        switch (state)
        {
        case THERMAL_STATE_WARN:
            announce("warn", "heat sink above warning threshold - reduce LED power");
            break;
        case THERMAL_STATE_CRITICAL:
            announce("critical", detail ? detail : "LED power cut to protect the hardware");
            break;
        case THERMAL_STATE_FAULT:
            announce("fault", detail);
            break;
        case THERMAL_STATE_OK:
            // Don't announce a "recovery" for the first normal window
            // after boot — there was nothing to recover from.
            if (announcedState != THERMAL_STATE_UNKNOWN)
                announce("recovered", "LED commands accepted again");
            break;
        default:
            break;
        }
        announcedState = state;
    }
}

void thermal_loop(void)
{
    if (!enabled || !haveAnyPin)
        return;

    uint32_t now = millis();
    if ((now - lastSweepMs) < THERMAL_SAMPLE_PERIOD_MS)
        return;
    lastSweepMs = now;

    // One sample per channel per sweep — a few hundred microseconds in
    // total, so this never stalls the main loop. Spreading the sweeps out
    // over ~640 ms is what averages the ±20-count scatter (and any mains
    // ripple) away, rather than a tight burst of reads.
    for (uint8_t i = 0; i < THERMAL_NUM_SENSORS; ++i)
    {
        if (readings[i].pin < 0)
            continue;
        accMv[i] += analogReadMilliVolts(readings[i].pin);
    }
    sweepCount++;

    if (sweepCount >= THERMAL_OVERSAMPLE)
    {
        // Raw counts are diagnostics only, so sample them once per window
        // rather than per sweep.
        for (uint8_t i = 0; i < THERMAL_NUM_SENSORS; ++i)
        {
            if (readings[i].pin >= 0)
                lastRaw[i] = (uint16_t)analogRead(readings[i].pin);
        }
        evaluateWindow();
        sweepCount = 0;
        for (uint8_t i = 0; i < THERMAL_NUM_SENSORS; ++i)
            accMv[i] = 0;
    }
}

// ── accessors ────────────────────────────────────────────────────────
float thermal_get_mean_c(void) { return enabled ? meanC : NAN; }
float thermal_get_max_c(void)  { return enabled ? maxC : NAN; }
bool  thermal_is_led_cut_active(void) { return enabled && ledCutActive; }
bool  thermal_is_enabled(void) { return enabled; }
int   thermal_get_state(void) { return enabled ? state : THERMAL_STATE_UNKNOWN; }

// ── JSON API ─────────────────────────────────────────────────────────
cJSON *thermal_get(cJSON *ob)
{
    (void)ob;
    cJSON *root = cJSON_CreateObject();
    cJSON *t = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "temp", t);

    cjson_tool_set_int(t, "enabled", enabled ? 1 : 0);
    cjson_tool_set_int(t, "available", haveAnyPin ? 1 : 0);
    cJSON_AddStringToObject(t, "state", stateName(state));
    cjson_tool_set_int(t, "ledCut", ledCutActive ? 1 : 0);
    if (!isnan(meanC))
        cjson_tool_set_float(t, "meanC", meanC);
    if (!isnan(maxC))
        cjson_tool_set_float(t, "maxC", maxC);
    cjson_tool_set_float(t, "warnC", warnC);
    cjson_tool_set_float(t, "criticalC", criticalC);
    cjson_tool_set_float(t, "recoverC", recoverC);

    cJSON *arr = cJSON_CreateArray();
    for (uint8_t i = 0; i < THERMAL_NUM_SENSORS; ++i)
    {
        cJSON *s = cJSON_CreateObject();
        cjson_tool_set_int(s, "gpio", readings[i].pin);
        cjson_tool_set_int(s, "mv", readings[i].milliVolts);
        cjson_tool_set_int(s, "raw", readings[i].raw);
        if (!isnan(readings[i].degC))
            cjson_tool_set_float(s, "degC", readings[i].degC);
        cjson_tool_set_int(s, "fault", readings[i].fault ? 1 : 0);
        cJSON_AddItemToArray(arr, s);
    }
    cJSON_AddItemToObject(t, "sensors", arr);
    return root;
}

// Only accept a numeric override; a string or a bool here would silently
// become 0.0 °C and disable the protection.
static bool readFloatField(cJSON *ob, const char *key, float *out)
{
    cJSON *v = cJSON_GetObjectItemCaseSensitive(ob, key);
    if (v == NULL || !cJSON_IsNumber(v))
        return false;
    *out = (float)v->valuedouble;
    return true;
}

int thermal_act(cJSON *ob)
{
    int qid = cjson_tool_get_int(ob, "qid", 0);
    bool changed = false;

    cJSON *jen = cJSON_GetObjectItemCaseSensitive(ob, "enabled");
    if (jen != NULL && (cJSON_IsNumber(jen) || cJSON_IsBool(jen)))
    {
        bool want = cjson_tool_get_bool(ob, "enabled", enabled);
        if (want && !haveAnyPin)
        {
            log_w("/temp_act: no thermistor pins configured on this board");
        }
        else
        {
            enabled = want;
            // Leaving the LEDs latched off after the user disabled the
            // protection would be a trap; re-enabling starts a fresh
            // "are the sensors there at all?" evaluation.
            ledCutActive = false;
            state = THERMAL_STATE_UNKNOWN;
            announcedState = THERMAL_STATE_UNKNOWN;
            if (enabled)
            {
                sawValidReading = false;
                sweepCount = 0;
                for (uint8_t i = 0; i < THERMAL_NUM_SENSORS; ++i)
                {
                    accMv[i] = 0;
                    lastRaw[i] = 0;
                }
                lastSweepMs = millis();
            }
            changed = true;
        }
    }

    if (readFloatField(ob, "warnC", &warnC))
        changed = true;
    if (readFloatField(ob, "criticalC", &criticalC))
        changed = true;
    if (readFloatField(ob, "recoverC", &recoverC))
        changed = true;

    if (cjson_tool_get_int(ob, "reset", 0) == 1)
    {
        warnC = pinConfig.thermal_warn_c;
        criticalC = pinConfig.thermal_critical_c;
        recoverC = pinConfig.thermal_recover_c;
        enabled = haveAnyPin && pinConfig.thermal_enabled_default;
        ledCutActive = false;
        state = THERMAL_STATE_UNKNOWN;
        announcedState = THERMAL_STATE_UNKNOWN;
        sawValidReading = false;
        changed = true;
    }

    if (changed)
    {
        persist();
        log_i("/temp_act: enabled=%d warn=%.1f crit=%.1f recover=%.1f",
              enabled, (double)warnC, (double)criticalC, (double)recoverC);
    }
    return qid;
}
