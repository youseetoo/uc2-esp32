#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "soc/rtc.h"

#ifndef ESP32S3_MODEL_XIAO
#include "driver/dac.h"
#endif

#ifdef ESP32S3_MODEL_XIAO
typedef enum {
    DAC_CHANNEL_1 = 0,    /*!< DAC channel 1 is GPIO25(ESP32) / GPIO17(ESP32S2) */
    DAC_CHANNEL_2 = 1,    /*!< DAC channel 2 is GPIO26(ESP32) / GPIO18(ESP32S2) */
    DAC_CHANNEL_MAX,
} dac_channel_t;
#endif

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
