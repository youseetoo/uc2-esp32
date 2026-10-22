// Host stub for esp_adc/adc_cali_scheme.h — calibration "unsupported" on host,
// so uc2_adc.cpp takes the naive full-scale fallback path.
#pragma once

#include <stdint.h>
#include <esp_err.h>
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *adc_cali_handle_t;

// Host: pretend the eFuse calibration is not available (matches the
// "eFuse not burnt" fallback branch in uc2_adc.cpp).
#define ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED 0

typedef struct {
    adc_unit_t unit_id;
    adc_channel_t chan;
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
} adc_cali_curve_fitting_config_t;

esp_err_t adc_cali_create_scheme_curve_fitting(
    const adc_cali_curve_fitting_config_t *config, adc_cali_handle_t *out_handle);
esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t handle, int raw, int *mv);
esp_err_t adc_cali_delete_scheme(adc_cali_handle_t handle);

#ifdef __cplusplus
}
#endif
