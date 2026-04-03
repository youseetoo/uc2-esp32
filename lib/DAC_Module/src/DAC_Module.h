#pragma once

// ESP32-S3 (Xiao) and M5Dial have no DAC peripheral — stub out the entire module.
#if defined(ESP32S3_MODEL_XIAO) || defined(M5DIAL)
// No DAC hardware available — provide empty stub so dependents still compile.
typedef enum { DAC_CHANNEL_1 = 0, DAC_CHANNEL_2 = 1, DAC_CHANNEL_MAX } dac_channel_t;
class DAC_Module {
public:
    void Stop(dac_channel_t) {}
    void Setup(dac_channel_t, int, int, int, int, int) {}
    void dac_offset_set(dac_channel_t, int) {}
};
#else
// Real ESP32 with DAC peripheral
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/dac.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"

class DAC_Module {
    public:
        void Stop(dac_channel_t channel);
        void Setup(dac_channel_t channel, int clk_div, int frequency, int scale, int phase, int invert);
        void dac_offset_set(dac_channel_t channel, int offset);
    private:
        void dac_cosine_enable(dac_channel_t channel, int invert);
        void dac_cosine_disable(dac_channel_t channel);
        void dac_frequency_set(int clk_8m_div, int frequency_step);
        void dac_scale_set(dac_channel_t channel, int scale);
        void dac_invert_set(dac_channel_t channel, int invert);
};
#endif
