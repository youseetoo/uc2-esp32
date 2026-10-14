// T1.3 (no Arduino): uc2_ledc — LEDC PWM façade over driver/ledc.h.
// API surface = exactly what main/ uses (grep-verified 2026-10-12):
// ledcSetup (1: LaserController), ledcAttachPin (1), ledcDetachPin (1),
// ledcWrite (3: LaserController x2, LedController).
//
// Arduino "channel" numbers (0-15) are used AS the IDF 6.x global LEDC
// channel index (ledc_channel_t 0..LEDC_CHANNEL_MAX-1; timer is implicit:
// S3 timer = ch>=8 ? 1 : 0). The explicit timer is only needed for
// ledc_timer_config / timer_sel (esp32-hal-ledc layout).
#pragma once

#include <cstdint>

#include <driver/ledc.h>

namespace
{
inline ledc_timer_t uc2_ledc_timer_for(int channel)
{
#if defined(CONFIG_IDF_TARGET_ESP32)
    return static_cast<ledc_timer_t>(channel / 4);
#else
    return static_cast<ledc_timer_t>(channel >= 8 ? 1 : 0);
#endif
}

// Identity map: IDF 6.x channels are already global indices.
inline ledc_channel_t uc2_ledc_ch_for(int channel)
{
    return static_cast<ledc_channel_t>(channel);
}

// pin → Arduino-channel bookkeeping (attach/detach). Function-local static =
// one shared table across all TUs.
inline int* uc2_ledc_pin_table()
{
    static int table[40];
    static bool init = false;
    if (!init)
    {
        for (int i = 0; i < 40; i++)
            table[i] = -1;
        init = true;
    }
    return table;
}
} // namespace

// Arduino ledcSetup(channel, freq, resolution): configure the timer bank the
// channel lives on. Returns true on success.
inline bool ledcSetup(int channel, uint32_t freq, uint8_t resolution)
{
    ledc_timer_config_t cfg = {};
    cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    cfg.timer_num = uc2_ledc_timer_for(channel);
    cfg.duty_resolution = static_cast<ledc_timer_bit_t>(resolution);
    cfg.freq_hz = freq;
    cfg.clk_cfg = LEDC_AUTO_CLK;
    cfg.deconfigure = false;
    return ledc_timer_config(&cfg) == ESP_OK;
}

// Arduino ledcAttachPin(pin, channel): bind the channel output to the pin.
inline bool ledcAttachPin(int pin, int channel)
{
    if (pin < 0 || pin >= 40)
        return false;
    uc2_ledc_pin_table()[pin] = channel;

    ledc_channel_config_t cfg = {};
    cfg.gpio_num = pin;
    cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    cfg.timer_sel = uc2_ledc_timer_for(channel);
    cfg.channel = uc2_ledc_ch_for(channel);
    cfg.duty = 0;
    cfg.hpoint = 0;
    cfg.sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD;
    cfg.flags.output_invert = 0;
    cfg.deconfigure = false;
    return ledc_channel_config(&cfg) == ESP_OK;
}

// Arduino ledcDetachPin(pin): release the pin from its LEDC channel.
inline bool ledcDetachPin(int pin)
{
    if (pin < 0 || pin >= 40)
        return false;
    const int channel = uc2_ledc_pin_table()[pin];
    uc2_ledc_pin_table()[pin] = -1;
    if (channel < 0)
        return false;

    ledc_channel_config_t cfg = {};
    cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    cfg.channel = uc2_ledc_ch_for(channel);
    cfg.deconfigure = true;  // gpio_num/timer_sel/duty/... ignored (IDF 6.1: ledc_channel_disable removed)
    return ledc_channel_config(&cfg) == ESP_OK;
}

// Arduino ledcWrite(channel, duty): set + update the duty in one call.
inline void ledcWrite(int channel, uint32_t duty)
{
    const ledc_channel_t ch = uc2_ledc_ch_for(channel);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

// Arduino ledcRead(channel): get current duty cycle.
// ledc_get_duty(speed_mode, channel) returns uint32_t in IDF 6.x.
inline uint32_t ledcRead(int channel)
{
    ledc_channel_t ch = uc2_ledc_ch_for(channel);
    return ledc_get_duty(LEDC_LOW_SPEED_MODE, ch);
}
