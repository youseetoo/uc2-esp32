// Host stub: Ws2812Rmt.h — in-memory pixel buffer (S3 RMT path).
#pragma once
#include <cstdint>
#include <cstddef>

class Ws2812Rmt {
public:
    enum Ws2812Type {
        Ws2812Type_GRB,
        Ws2812Type_GRBW,
        Ws2812Type_RGB,
        Ws2812Type_RGBW
    };

    Ws2812Rmt(uint8_t pin, uint16_t leds, Ws2812Type type = Ws2812Type_GRB,
              bool dataInversion = false)
        : m_pin(pin), m_leds(leds), m_type(type) {
        m_data = new uint32_t[leds + 1]();
    }
    ~Ws2812Rmt() { delete[] m_data; }

    void begin() { m_began = true; }
    void update() { m_updateCount++; }
    // Static host-side "transmit" used by LedController on USE_FASTACCEL builds:
    // pixels is the raw GRB buffer; nothing to do on the host.
    static void show(uint8_t* pixels, size_t count) { (void)pixels; (void)count; }
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
        if (n < m_leds) m_data[n] = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    }
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        setPixelColor(n, r, g, b); (void)w;
    }
    void setPixelColor(uint16_t n, uint32_t c) { if (n < m_leds) m_data[n] = c; }
    uint32_t getPixelColor(uint16_t n) const { return n < m_leds ? m_data[n] : 0; }
    void setBrightness(uint8_t b) { m_brightness = b; }
    uint8_t getBrightness() const { return m_brightness; }
    void clear() { for (uint16_t i = 0; i < m_leds; ++i) m_data[i] = 0; }
    uint16_t count() const { return m_leds; }
    bool isBegan() const { return m_began; }
    uint16_t getUpdateCount() const { return m_updateCount; }
    uint8_t getPin() const { return m_pin; }
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    }
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        return Color(r, g, b) | ((uint32_t)w << 24);
    }

private:
    uint8_t m_pin;
    uint16_t m_leds;
    Ws2812Type m_type;
    uint8_t m_brightness = 255;
    bool m_began = false;
    uint16_t m_updateCount = 0;
    uint32_t* m_data;
};
