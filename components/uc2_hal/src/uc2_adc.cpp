// T2.3: Arduino-compatible ADC shim over ESP-IDF 6.1 esp_adc oneshot driver.
// Used by GpioCanSlave.cpp (collision detection ADC reading).
#include "uc2_hal/uc2_adc.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali_scheme.h>
#include <hal/adc_types.h>

static const char *TAG = "uc2_adc";

// ── State ───────────────────────────────────────────────────────────────────
static adc_oneshot_unit_handle_t s_unit[2] = {nullptr, nullptr};  // [0]=ADC1, [1]=ADC2
static int s_resolution = 12;  // Arduino default: 12-bit

// Per-pin attenuation (stored as Arduino value; -1 = not set)
#define MAX_PINS 20
static int s_atten[MAX_PINS] = {};

// ── eFuse calibration (analogReadMilliVolts) ─────────────────────────────────
// One curve-fitting scheme handle per (unit, channel, attenuation) combo.
// Key = (unit*8 + chan)*4 + atten. s_caliTried && !s_cali = creation failed
// (eFuse not burnt) — fall back to the naive full-scale conversion.
#define CALI_MAX 16
static adc_cali_handle_t s_cali[CALI_MAX] = {};
static bool s_caliTried[CALI_MAX] = {};

// Full scale in mV per Arduino attenuation index (fallback scale only).
static int attenFullScaleMv(int att)
{
    switch (att) {
        case ADC_6db:   return 1600;
        case ADC_2_5db: return 1000;
        case ADC_0db:   return 800;
        default:        return 3300;  // ADC_11db -> 12 dB on S3
    }
}

static adc_cali_handle_t getCaliHandle(adc_unit_t unit, adc_channel_t chan, int attenIdx)
{
    int key = ((int)unit * 8 + (int)chan) * 4 + attenIdx;
    if (key < 0 || key >= CALI_MAX) return nullptr;
    if (s_caliTried[key] && !s_cali[key]) return nullptr;
    if (s_cali[key]) return s_cali[key];
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id  = unit,
        .chan     = chan,   // S3: per-channel calibration
        .atten    = arduinoAttToIdf(attenIdx),
        .bitwidth = (adc_bitwidth_t)ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_curve_fitting(&cfg, &s_cali[key]) == ESP_OK)
        return s_cali[key];
    ESP_LOGW(TAG, "adc_cali curve fitting unavailable (unit %d chan %d) — naive scale",
             (int)unit, (int)chan);
#else
    (void)unit; (void)chan; (void)attenIdx;
#endif
    s_caliTried[key] = true;
    return nullptr;
}

// ── Helpers ─────────────────────────────────────────────────────────────────
static inline adc_atten_t arduinoAttToIdf(int att)
{
    switch (att) {
        case ADC_11db:  return ADC_ATTEN_DB_12;   // max attenuation
        case ADC_6db:   return ADC_ATTEN_DB_6;
        case ADC_2_5db: return ADC_ATTEN_DB_2_5;
        case ADC_0db:   return ADC_ATTEN_DB_0;
        default:        return ADC_ATTEN_DB_12;
    }
}

static esp_err_t ensureUnit(adc_unit_t unit)
{
    int idx = (unit == ADC_UNIT_1) ? 0 : 1;
    if (s_unit[idx]) return ESP_OK;

    adc_oneshot_unit_init_cfg_t cfg = {
        .unit_id  = unit,
        .clk_src  = (adc_oneshot_clk_src_t)SOC_MOD_CLK_RC_FAST,  // S3: RTC ctrl; other SoCs: cast is safe
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t err = adc_oneshot_new_unit(&cfg, &s_unit[idx]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit(%d) failed: %s", (int)unit, esp_err_to_name(err));
    }
    return err;
}

// ── Public API ──────────────────────────────────────────────────────────────
void analogReadResolution(int bits)
{
    if (bits > 0 && bits <= 12) s_resolution = bits;
}

void analogSetPinAttenuation(int pin, int attenuation)
{
    if (pin < 0 || pin >= MAX_PINS) return;
    s_atten[pin] = attenuation;

    adc_unit_t unit;
    adc_channel_t chan;
    if (adc_oneshot_io_to_channel(pin, &unit, &chan) != ESP_OK) return;

    if (ensureUnit(unit) != ESP_OK) return;

    adc_oneshot_chan_cfg_t chcfg = {
        .atten = arduinoAttToIdf(attenuation),
        .bitwidth = (adc_bitwidth_t)ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(s_unit[(unit == ADC_UNIT_1) ? 0 : 1], chan, &chcfg);
}

int analogRead(int pin)
{
    if (pin < 0 || pin >= MAX_PINS) return 0;

    adc_unit_t unit;
    adc_channel_t chan;
    if (adc_oneshot_io_to_channel(pin, &unit, &chan) != ESP_OK) return 0;

    if (ensureUnit(unit) != ESP_OK) return 0;

    // If attenuation was never set, use default (max attenuation like Arduino)
    if (s_atten[pin] < 0) {
        s_atten[pin] = ADC_11db;
        adc_oneshot_chan_cfg_t chcfg = {
            .atten = arduinoAttToIdf(ADC_11db),
            .bitwidth = (adc_bitwidth_t)ADC_BITWIDTH_12,
        };
        adc_oneshot_config_channel(s_unit[(unit == ADC_UNIT_1) ? 0 : 1], chan, &chcfg);
    }

    int raw = 0;
    if (adc_oneshot_read(s_unit[(unit == ADC_UNIT_1) ? 0 : 1], chan, &raw) != ESP_OK) {
        return 0;
    }

    // Scale to requested resolution (Arduino-ESP32 behaviour)
    if (s_resolution < 12) {
        int shift = 12 - s_resolution;
        raw >>= shift;
    }
    return raw;
}

int analogReadMilliVolts(int pin)
{
    if (pin < 0 || pin >= MAX_PINS) return 0;

    adc_unit_t unit;
    adc_channel_t chan;
    if (adc_oneshot_io_to_channel(pin, &unit, &chan) != ESP_OK) return 0;

    if (ensureUnit(unit) != ESP_OK) return 0;
    int idx = (unit == ADC_UNIT_1) ? 0 : 1;

    // Same lazy default as analogRead() (Arduino applies max attenuation).
    if (s_atten[pin] < 0) {
        s_atten[pin] = ADC_11db;
        adc_oneshot_chan_cfg_t chcfg = {
            .atten = arduinoAttToIdf(ADC_11db),
            .bitwidth = (adc_bitwidth_t)ADC_BITWIDTH_12,
        };
        adc_oneshot_config_channel(s_unit[idx], chan, &chcfg);
    }

    int raw = 0;
    if (adc_oneshot_read(s_unit[idx], chan, &raw) != ESP_OK) return 0;

    adc_cali_handle_t cal = getCaliHandle(unit, chan, s_atten[pin]);
    if (cal) {
        int mv = 0;
        if (adc_cali_raw_to_voltage(cal, raw, &mv) == ESP_OK) return mv;
    }
    // Naive fallback (no eFuse calibration): Arduino-ESP32 reads ~8-10 % low
    // on S3 — good enough to keep a missing eFuse from reading 0 mV.
    return raw * attenFullScaleMv(s_atten[pin]) / 4095;
}
