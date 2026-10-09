// Host stub: Adafruit_NeoPixel.h — in-memory pixel buffer.
#pragma once
#include <cstdint>
#include <cstddef>

#define NEO_GRB 0x00000020
#define NEO_RGB 0x00000040
#define NEO_GRBW 0x00000080
#define NEO_RGBW 0x00000100
#define NEO_KHZ800 0x00000000
#define NEO_KHZ400 0x00000200
#define NEO_ORDER_GRB 0
#define NEO_ORDER_RGB 1

class Adafruit_NeoPixel {
public:
    Adafruit_NeoPixel(uint16_t n, uint8_t pin = 6, uint8_t type = NEO_GRB | NEO_KHZ800)
        : m_n(n), m_pin(pin), m_type(type) {
        m_data = new uint32_t[n + 1];
        m_width = new uint8_t[n + 1];
        for (uint16_t i = 0; i <= n; ++i) { m_data[i] = 0; m_width[i] = 255; }
    }
    ~Adafruit_NeoPixel() { delete[] m_data; delete[] m_width; }

    void begin() { m_began = true; }
    void show() { m_showCount++; }
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
        if (n < m_n) m_data[n] = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    }
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        setPixelColor(n, r, g, b); (void)w;
    }
    void setPixelColor(uint16_t n, uint32_t c) {
        if (n < m_n) m_data[n] = c;
    }
    uint32_t getPixelColor(uint16_t n) const { return n < m_n ? m_data[n] : 0; }
    void setBrightness(uint8_t b) { m_brightness = b; }
    uint8_t getBrightness() const { return m_brightness; }
    void clear() { for (uint16_t i = 0; i < m_n; ++i) m_data[i] = 0; }
    uint8_t* getPixels() { return (uint8_t*)m_data; }
    const uint8_t* getPixels() const { return (const uint8_t*)m_data; }
    void fill(uint32_t c) { for (uint16_t i = 0; i < m_n; ++i) m_data[i] = c; }
    uint16_t numPixels() const { return m_n; }
    bool isBegan() const { return m_began; }
    uint16_t getShowCount() const { return m_showCount; }
    uint8_t getPin() const { return m_pin; }
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    }
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        return Color(r, g, b) | ((uint32_t)w << 24);
    }
    // HSV -> RGB (Adafruit algorithm, gamma32 off — host fidelity is enough).
    static uint32_t ColorHSV(uint32_t hue, uint8_t sat, uint8_t val) {
        uint8_t r = 0, g = 0, b = 0;
        hue %= 65536;
        hue = (hue * 3) & 0xFFFF;  // 0..65535
        if (hue < 2048) {
            r = 255 - (hue >> 3);
            g = (hue >> 3);
            b = 0;
        } else if (hue < 4096) {
            hue -= 2048;
            r = 0;
            g = 255 - (hue >> 3);
            b = (hue >> 3);
        } else if (hue < 6144) {
            hue -= 4096;
            r = (hue >> 3);
            g = 0;
            b = 255 - (hue >> 3);
        } else if (hue < 8192) {
            hue -= 6144;
            r = 255 - (hue >> 3);
            g = 0;
            b = (hue >> 3);
        } else if (hue < 10240) {
            hue -= 8192;
            r = 0;
            g = (hue >> 3);
            b = 255 - (hue >> 3);
        } else {
            hue -= 10240;
            r = 0;
            g = 255 - (hue >> 3);
            b = (hue >> 3);
        }
        if (sat < 255) {
            uint8_t t = 255 - sat;
            r = r + (((255 - r) * t) >> 8);
            g = g + (((255 - g) * t) >> 8);
            b = b + (((255 - b) * t) >> 8);
        }
        if (val < 255) {
            r = (r * val) >> 8;
            g = (g * val) >> 8;
            b = (b * val) >> 8;
        }
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    // Adafruit has both a `bool gamma32` member and a gamma32(c) method;
    // the code under test only calls the method — identity on host.
    static uint32_t gamma32(uint32_t c) { return c; }
    // Per-channel 8-bit gamma (identity on host).
    static uint8_t gamma8(uint8_t c) { return c; }

private:
    uint16_t m_n;
    uint8_t m_pin;
    uint8_t m_type;
    uint8_t m_brightness = 255;
    bool m_began = false;
    uint16_t m_showCount = 0;
    uint32_t* m_data;
    uint8_t* m_width;
};
