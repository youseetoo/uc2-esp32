// Host stub for esp_adc/adc_oneshot.h — no-op driver surface for uc2_adc.cpp.
#pragma once

#include <stdint.h>
#include <esp_err.h>
#include "hal/adc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *adc_oneshot_unit_handle_t;

typedef enum {
    ADC_ULP_MODE_DISABLE = 0,
    ADC_ULP_MODE_ENABLE,
} adc_oneshot_clk_src_t;

typedef struct {
    adc_unit_t unit_id;
    adc_oneshot_clk_src_t clk_src;
    int ulp_mode;
} adc_oneshot_unit_init_cfg_t;

typedef struct {
    adc_atten_t atten;
    adc_bitwidth_t bitwidth;
} adc_oneshot_chan_cfg_t;

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *init_cfg,
                               adc_oneshot_unit_handle_t *ret_handle);
esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t handle);
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle,
                                     adc_channel_t channel,
                                     const adc_oneshot_chan_cfg_t *config);
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle,
                           adc_channel_t channel, int *value);
esp_err_t adc_oneshot_io_to_channel(int gpio_number, adc_unit_t *unit,
                                    adc_channel_t *channel);

#ifdef __cplusplus
}
#endif
