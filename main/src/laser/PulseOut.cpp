#include "PulseOut.h"
#include "Arduino.h"
#include "driver/timer.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "esp_rom_sys.h"

// Runs inside the IDF timer ISR (which holds the timer spinlock, so the
// busy-waited edge cannot be stretched by another interrupt on this core).
// The IDF ISR re-enables the alarm only when the callback changed the alarm
// value, so a finished pulse never retriggers.
static bool IRAM_ATTR pulseIsr(void *arg)
{
    return static_cast<PulseOut *>(arg)->onAlarm();
}

void IRAM_ATTR PulseOut::setActive(bool active)
{
    gpio_ll_set_level(&GPIO, (gpio_num_t)pin_, (active == activeHigh_) ? 1 : 0);
}

bool IRAM_ATTR PulseOut::onAlarm()
{
    const timer_group_t g = (timer_group_t)group_;
    const timer_idx_t t = (timer_idx_t)index_;
    if (phase_ == WaitHigh)
    {
        setActive(true);
        const uint32_t w = widthUs_;
        if (w <= kBusyWaitMaxUs)
        {
            esp_rom_delay_us(w);
            setActive(false);
            phase_ = Idle;
            fired_ = fired_ + 1;
            timer_group_set_counter_enable_in_isr(g, t, TIMER_PAUSE);
        }
        else
        {
            const uint64_t now = timer_group_get_counter_value_in_isr(g, t);
            timer_group_set_alarm_value_in_isr(g, t, now + w);
            phase_ = WaitLow;
        }
    }
    else if (phase_ == WaitLow)
    {
        setActive(false);
        phase_ = Idle;
        fired_ = fired_ + 1;
        timer_group_set_counter_enable_in_isr(g, t, TIMER_PAUSE);
    }
    return false;
}

bool PulseOut::begin(int timerGroup, int timerIndex, int pin, bool activeHigh)
{
    if (pin < 0 || timerGroup < 0 || timerGroup >= TIMER_GROUP_MAX || timerIndex < 0 || timerIndex >= TIMER_MAX)
        return false;
    if (started_ && (timerGroup != group_ || timerIndex != index_))
        end();

    pin_ = pin;
    activeHigh_ = activeHigh;
    pinMode(pin_, OUTPUT);
    setActive(false);

    if (!started_)
    {
        timer_config_t cfg = {};
        cfg.alarm_en = TIMER_ALARM_EN;
        cfg.counter_en = TIMER_PAUSE;
        cfg.intr_type = TIMER_INTR_LEVEL;
        cfg.counter_dir = TIMER_COUNT_UP;
        cfg.auto_reload = TIMER_AUTORELOAD_DIS;
        cfg.divider = 80; // 80 MHz APB -> 1 µs ticks
        const timer_group_t g = (timer_group_t)timerGroup;
        const timer_idx_t t = (timer_idx_t)timerIndex;
        if (timer_init(g, t, &cfg) != ESP_OK)
        {
            log_e("PulseOut: timer_init(%d,%d) failed", timerGroup, timerIndex);
            return false;
        }
        timer_set_counter_value(g, t, 0);
        timer_enable_intr(g, t);
        if (!isrInstalled_)
        {
            if (timer_isr_callback_add(g, t, pulseIsr, this, ESP_INTR_FLAG_IRAM) != ESP_OK)
            {
                log_e("PulseOut: timer_isr_callback_add(%d,%d) failed", timerGroup, timerIndex);
                timer_deinit(g, t);
                return false;
            }
            isrInstalled_ = true;
        }
        group_ = timerGroup;
        index_ = timerIndex;
        started_ = true;
    }
    phase_ = Idle;
    return true;
}

void PulseOut::end()
{
    if (!started_)
        return;
    const timer_group_t g = (timer_group_t)group_;
    const timer_idx_t t = (timer_idx_t)index_;
    timer_pause(g, t);
    if (isrInstalled_)
    {
        timer_isr_callback_remove(g, t);
        isrInstalled_ = false;
    }
    timer_deinit(g, t);
    phase_ = Idle;
    if (pin_ >= 0)
        setActive(false);
    started_ = false;
}

bool PulseOut::fire(uint32_t delayUs, uint32_t widthUs)
{
    if (!started_ || widthUs == 0 || phase_ != Idle)
        return false;
    const timer_group_t g = (timer_group_t)group_;
    const timer_idx_t t = (timer_idx_t)index_;
    widthUs_ = widthUs;
    timer_pause(g, t);
    timer_set_counter_value(g, t, 0);
    if (delayUs < 2)
    {
        // No meaningful delay: raise now, let the timer end the pulse.
        phase_ = WaitLow;
        setActive(true);
        timer_set_alarm_value(g, t, widthUs);
    }
    else
    {
        phase_ = WaitHigh;
        timer_set_alarm_value(g, t, delayUs);
    }
    timer_set_alarm(g, t, TIMER_ALARM_EN);
    timer_start(g, t);
    return true;
}
