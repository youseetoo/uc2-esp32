// Based on https://github.com/krzychb/dac-cosine/blob/master/main/dac-cosine.c

#if SOC_DAC_SUPPORTED
#if !defined(M5DIAL)
#include "DAC_Module.h"

DAC_Module DAC_Module;

// IDF 6.x removed dac_output_enable/disable + dac_write_byte. Shim the DAC output
// onto the oneshot channel API (creating a channel == enabling that output).
static dac_oneshot_handle_t s_oneshot[2] = { NULL, NULL };
static void dac_out_enable(dac_channel_t channel) {
    int idx = (channel == DAC_CHANNEL_2) ? 1 : 0;
    if (!s_oneshot[idx]) {
        dac_oneshot_config_t cfg = { .chan_id = (dac_channel_t)idx };
        if (dac_oneshot_new_channel(&cfg, &s_oneshot[idx]) != ESP_OK) s_oneshot[idx] = NULL;
    }
}
static void dac_out_disable(dac_channel_t channel) {
    int idx = (channel == DAC_CHANNEL_2) ? 1 : 0;
    if (s_oneshot[idx]) { dac_oneshot_del_channel(s_oneshot[idx]); s_oneshot[idx] = NULL; }
}

void dacWrite(int channel, int value) {
    int idx = (channel == DAC_CHANNEL_2) ? 1 : 0;
    if (!s_oneshot[idx]) {
        dac_oneshot_config_t cfg = { .chan_id = (dac_channel_t)idx };
        if (dac_oneshot_new_channel(&cfg, &s_oneshot[idx]) != ESP_OK) return;
    }
    if (s_oneshot[idx]) dac_oneshot_output_voltage(s_oneshot[idx], (uint8_t)value);
}

void DAC_Module::Stop(dac_channel_t channel) {
    #if SOC_DAC_SUPPORTED
    dac_out_disable(channel);
    dac_cosine_disable(channel);
    #endif

}

/*
 * Enable cosine waveform generator on a DAC channel
 */
void DAC_Module::dac_cosine_enable(dac_channel_t channel, int invert)
{    
    #if SOC_DAC_SUPPORTED
    // Enable tone generator common to both channels
    SET_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);
    switch(channel) {
        case DAC_CHANNEL_1:
            // Enable / connect tone tone generator on / to this channel
            SET_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
            // Invert MSB, otherwise part of waveform will have inverted
            SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_INV1, invert, SENS_DAC_INV1_S);
            break;
        case DAC_CHANNEL_2:
            SET_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);
            SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_INV2, invert, SENS_DAC_INV2_S);
            break;
        default: break;
    }
    #endif

}

/*
 * Disables cosine waveform generator on a DAC channel
 */
void DAC_Module::dac_cosine_disable(dac_channel_t channel)
{    
    #if SOC_DAC_SUPPORTED
    // Disable tone generator common to both channels
    // CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);
    switch(channel) {
        case DAC_CHANNEL_1:
            // Disable / connect tone tone generator on / to this channel
            CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
            break;
        case DAC_CHANNEL_2:
            CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);
            break;
        default: break;
    }
    #endif
}

/*
 * Set frequency of internal CW generator common to both DAC channels
 *
 * clk_8m_div: 0b000 - 0b111
 * frequency: range 0x0001 - 0xFFFF
 *
 */
void DAC_Module::dac_frequency_set(int clk_8m_div, int frequency)
{
    #if SOC_DAC_SUPPORTED
    REG_SET_FIELD(RTC_CNTL_CLK_CONF_REG, RTC_CNTL_CK8M_DIV_SEL, clk_8m_div);
    SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL1_REG, SENS_SW_FSTEP, frequency, SENS_SW_FSTEP_S);
    #endif
}


/*
 * Scale output of a DAC channel using two bit pattern:
 *
 * - 00: no scale
 * - 01: scale to 1/2
 * - 10: scale to 1/4
 * - 11: scale to 1/8
 *
 */
void DAC_Module::dac_scale_set(dac_channel_t channel, int scale)
{
    #if SOC_DAC_SUPPORTED
    switch(channel) {
        case DAC_CHANNEL_1:
            SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_SCALE1, scale, SENS_DAC_SCALE1_S);
            break;
        case DAC_CHANNEL_2:
            SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_SCALE2, scale, SENS_DAC_SCALE2_S);
            break;
        default :
           printf("Channel %d\n", channel);
    }
    #endif
}

/*
 * Offset output of a DAC channel
 *
 * Range 0x00 - 0xFF
 *
 */
void DAC_Module::dac_offset_set(dac_channel_t channel, int offset)
{
    #if SOC_DAC_SUPPORTED
    switch(channel) {
        case DAC_CHANNEL_1:
            SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_DC1, offset, SENS_DAC_DC1_S);
            break;
        case DAC_CHANNEL_2:
            SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_DC2, offset, SENS_DAC_DC2_S);
            break;
        default :
           printf("Channel %d\n", channel);
    }
    #endif
}

/*
 * Invert output pattern of a DAC channel
 *
 * - 00: does not invert any bits,
 * - 01: inverts all bits,
 * - 10: inverts MSB,
 * - 11: inverts all bits except for MSB
 *
 */
void DAC_Module::dac_invert_set(dac_channel_t channel, int invert)
{
    #if SOC_DAC_SUPPORTED
    switch(channel) {
        case DAC_CHANNEL_1:
            SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_INV1, invert, SENS_DAC_INV1_S);
            break;
        case DAC_CHANNEL_2:
            SET_PERI_REG_BITS(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_INV2, invert, SENS_DAC_INV2_S);
            break;
        default :
           printf("Channel %d\n", channel);
    }
    #endif
}

void DAC_Module::Setup(dac_channel_t channel, int clk_div, int frequency, int scale, int phase, int invert) {

    #if SOC_DAC_SUPPORTED
    // frequency setting is common to both channels
    dac_frequency_set(clk_div, frequency);

    dac_scale_set(channel, scale);
    dac_offset_set(channel, phase);

    dac_cosine_enable(channel, invert);    
    dac_out_enable(channel);
    #endif
}
#endif
#endif