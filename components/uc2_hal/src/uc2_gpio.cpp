// T1.3 (no Arduino): GPIO + digital interrupt façade (see uc2_gpio.h).
// One lazily-installed ISR service + a small pin→callback table.
#include "uc2_hal/uc2_gpio.h"

#include <driver/gpio.h>

namespace
{
constexpr uint8_t kMaxIntPins = 8;

struct Entry
{
    int pin = -1;
    void (*fn)(void) = nullptr;
};

Entry g_entries[kMaxIntPins];
bool g_isrInstalled = false;

IRAM_ATTR void sharedGpioIr(void* arg)
{
    // The 6.1 ISR service already cleared this pin's status bit before
    // dispatching (and re-clears on exit) — nothing to do here.
    // `arg` is the Entry for the pin that fired (set at attach time).
    auto* e = static_cast<Entry*>(arg);
    if (e->pin >= 0 && e->fn) e->fn();
}
}  // namespace

void pinMode(uint8_t pin, uint8_t mode)
{
    if (pin >= SOC_GPIO_PIN_COUNT) return;
    gpio_config_t c = {};
    c.pin_bit_mask = 1ULL << pin;
    c.mode = GPIO_MODE_INPUT;
    c.pull_up_en = GPIO_PULLUP_DISABLE;
    c.pull_down_en = GPIO_PULLDOWN_DISABLE;
    switch (mode)
    {
    case OUTPUT:
        c.mode = GPIO_MODE_OUTPUT;
        break;
    case INPUT_PULLUP:
        c.mode = GPIO_MODE_INPUT;
        c.pull_up_en = GPIO_PULLUP_ENABLE;
        break;
    case INPUT_PULLDOWN:
        c.mode = GPIO_MODE_INPUT;
        c.pull_down_en = GPIO_PULLDOWN_ENABLE;
        break;
    case INPUT:
    default:
        break;
    }
    gpio_config(&c);
}

void digitalWrite(uint8_t pin, uint8_t value)
{
    if (pin >= SOC_GPIO_PIN_COUNT) return;
    gpio_set_level((gpio_num_t)pin, value ? 1 : 0);
}

int digitalRead(uint8_t pin)
{
    if (pin >= SOC_GPIO_PIN_COUNT) return 0;
    return gpio_get_level((gpio_num_t)pin);
}

void attachInterrupt(uint8_t pin, void (*fn)(void), uint8_t edge)
{
    if (pin >= SOC_GPIO_PIN_COUNT || fn == nullptr) return;

    int slot = -1;
    for (auto &e : g_entries)
    {
        if (e.pin == (int)pin) { slot = (int)(&e - &g_entries[0]); break; }
        if (slot < 0 && e.pin < 0) slot = (int)(&e - &g_entries[0]);
    }
    if (slot < 0) return;  // table full
    g_entries[slot].pin = pin;
    g_entries[slot].fn = fn;

    if (!g_isrInstalled)
    {
        // Legacy service API (present in IDF 6.1 esp_driver_gpio); 0 =
        // ESP_INTR_FLAG_DEFAULT (CPU0, no priority pinning).
        gpio_install_isr_service(0);
        g_isrInstalled = true;
    }

    gpio_int_type_t t;  // 6.1 names: POSEDGE / NEGEDGE / ANYEDGE / DISABLE
    switch (edge)
    {
    case RISING: t = GPIO_INTR_POSEDGE; break;
    case CHANGE: t = GPIO_INTR_ANYEDGE; break;
    case FALLING:
    default:    t = GPIO_INTR_NEGEDGE; break;
    }
    gpio_set_intr_type((gpio_num_t)pin, t);
    gpio_isr_handler_add((gpio_num_t)pin, sharedGpioIr, &g_entries[slot]);
    gpio_intr_enable((gpio_num_t)pin);
}

void detachInterrupt(uint8_t pin)
{
    if (pin >= SOC_GPIO_PIN_COUNT) return;
    for (auto &e : g_entries)
    {
        if (e.pin == (int)pin)
        {
            e.pin = -1;
            e.fn = nullptr;
        }
    }
    gpio_isr_handler_remove((gpio_num_t)pin);
    gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
}
