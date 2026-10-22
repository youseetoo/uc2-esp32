// Host stub for hal/adc_types.h — just enough for uc2_adc.cpp to compile.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADC_UNIT_1 = 1,
    ADC_UNIT_2 = 2,
} adc_unit_t;

typedef enum {
    ADC_CHANNEL_0 = 0,
    ADC_CHANNEL_1,
    ADC_CHANNEL_2,
    ADC_CHANNEL_3,
    ADC_CHANNEL_4,
    ADC_CHANNEL_5,
    ADC_CHANNEL_6,
    ADC_CHANNEL_7,
    ADC_CHANNEL_MAX,
} adc_channel_t;

typedef enum {
    ADC_ATTEN_DB_11 = 0,
    ADC_ATTEN_DB_12,
    ADC_ATTEN_DB_2_5,
    ADC_ATTEN_DB_6,
    ADC_ATTEN_DB_0,
} adc_atten_t;

typedef enum {
    ADC_BITWIDTH_9 = 9,
    ADC_BITWIDTH_11,
    ADC_BITWIDTH_12,
} adc_bitwidth_t;

#define SOC_MOD_CLK_RC_FAST 2

#ifdef __cplusplus
}
#endif
