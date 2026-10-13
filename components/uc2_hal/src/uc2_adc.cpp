// T2.3: Arduino-compatible ADC shim over ESP-IDF 6.1 esp_adc oneshot driver.
// Used by GpioCanSlave.cpp (collision detection ADC reading).
#include "uc2_hal/uc2_adc.h"

#include <esp_log.h>
#include <esp_err.h>
#include <esp_adc/adc_oneshot.h>
#include <hal/adc_types.h>

static const char *TAG = "uc2_adc";

// ── State ───────────────────────────────────────────────────────────────────
static adc_oneshot_unit_handle_t s_unit[2] = {nullptr, nullptr};  // [0]=ADC1, [1]=ADC2
static int s_resolution = 12;  // Arduino default: 12-bit

// Per-pin attenuation (stored as Arduino value; -1 = not set)
#define MAX_PINS 20
static int s_atten[MAX_PINS] = {};

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
