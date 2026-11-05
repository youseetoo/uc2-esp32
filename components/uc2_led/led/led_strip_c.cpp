// led_strip_c.cpp — C++ side of the led_strip_c.h shim (T4.1/T5.3).
//
// The strip variant is selected at build time: DOTSTAR (Adafruit_DotStar),
// HUB75 (Adafruit_Protomatter), or WS2812 (pure C RMT driver, T5.3).
// The Adafruit_NeoPixel dependency has been replaced with the ws2812_rmt
// bare-metal C driver (ws2812_rmt.c) for the WS2812 path.
#include "led_strip_c.h"
#include "PinConfig.h"

#define LED_COUNT (pinConfig.MATRIX_W * pinConfig.MATRIX_H)

#if defined(DOTSTAR)
#include "Adafruit_DotStar.h"
#elif defined(HUB75)
#include "Adafruit_Protomatter.h"
#else
// T5.3: Pure C WS2812 driver (replaces Adafruit_NeoPixel).
#include "Ws2812Rmt.h"
#include <cstdlib>
#include <string>
#endif

namespace {

#if defined(DOTSTAR)
using Strip = Adafruit_DotStar;
Strip *asStrip(void *handle) {
  return static_cast<Strip *>(handle);
}
#elif defined(HUB75)
using Strip = Adafruit_Protomatter;
Strip *asStrip(void *handle) {
  return static_cast<Strip *>(handle);
}
#endif

// RMT channel selection moved here from LedController.cpp (T4.1):
int rmtChannelFor(int pin) {
  if (pin != pinConfig.LED_PIN) {
    return 0;
  }
  const char *envOverride = std::getenv("UC2_WS2812_RMT_CHANNEL");
  if (!envOverride) {
    return 0;
  }
  std::string env(envOverride);
  if (env == "auto") {
    return 0;
  }
  int parsed = std::atoi(env.c_str());
  return (parsed >= 0 && parsed <= 15) ? parsed : 0;
}

} // namespace

extern "C" {

void *led_strip_create(void) {
#if defined(DOTSTAR)
  return new Strip(LED_COUNT, pinConfig.LED_PIN, pinConfig.LED_CLK, DOTSTAR_BGR);
#elif defined(HUB75)
  return new Strip(
      pinConfig.MATRIX_W, pinConfig.HUB75_BIT_DEPTH, 1,
      const_cast<uint8_t *>(pinConfig.HUB75_RGB_PINS), 5,
      const_cast<uint8_t *>(pinConfig.HUB75_ADDR_PINS), pinConfig.HUB75_CLK,
      pinConfig.HUB75_LAT, pinConfig.HUB75_OE, true);
#else
  // T5.3: WS2812 uses the pure C ws2812_rmt driver (no C++ object).
  // Allocate pixel buffer on the heap (3 bytes per pixel).
  void *buf = calloc(LED_COUNT, 3);
  if (!buf) {
    return nullptr;
  }
  return buf;
#endif
}

bool led_strip_begin(void *handle) {
#if defined(DOTSTAR) || defined(HUB75)
  Strip *strip = asStrip(handle);
  strip->begin();
  return true;
#else
  // T5.3: WS2812 uses the pure C ws2812_rmt driver.
  return ws2812_rmt_begin(pinConfig.LED_PIN, rmtChannelFor(pinConfig.LED_PIN));
#endif
}

void led_strip_show(void *handle) {
#if defined(DOTSTAR) || defined(HUB75)
  asStrip(handle)->show();
#else
  // T5.3: WS2812 uses the pure C ws2812_rmt driver.
  // The pixel buffer is the handle itself.
  ws2812_rmt_show((const uint8_t *)handle, LED_COUNT * 3);
#endif
}

void led_strip_set_pixel(void *handle, uint16_t index, uint32_t color) {
#if defined(DOTSTAR) || defined(HUB75)
  asStrip(handle)->setPixelColor(index, color);
#else
  // T5.3: WS2812 uses the pure C ws2812_rmt driver.
  // Pixel data is stored in the handle buffer (GRB order, 3 bytes per pixel).
  uint8_t *pixels = (uint8_t *)handle;
  pixels[index * 3 + 0] = (color >> 16) & 0xFF; // R
  pixels[index * 3 + 1] = (color >> 8) & 0xFF;  // G
  pixels[index * 3 + 2] = color & 0xFF;         // B
#endif
}

void led_strip_clear(void *handle) {
#if defined(HUB75)
  asStrip(handle)->fillScreen(0);
#else
#if defined(DOTSTAR) || defined(HUB75)
  asStrip(handle)->clear();
#else
  // T5.3: WS2812 uses the pure C ws2812_rmt driver.
  memset(handle, 0, LED_COUNT * 3);
#endif
#endif
}

void led_strip_fill(void *handle, uint32_t color) {
#if defined(HUB75)
  asStrip(handle)->fillScreen(color);
#else
#if defined(DOTSTAR) || defined(HUB75)
  asStrip(handle)->clear();
  asStrip(handle)->fill(color);
#else
  // T5.3: WS2812 uses the pure C ws2812_rmt driver.
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  uint8_t *pixels = (uint8_t *)handle;
  for (int i = 0; i < LED_COUNT; i++) {
    pixels[i * 3 + 0] = r;
    pixels[i * 3 + 1] = g;
    pixels[i * 3 + 2] = b;
  }
#endif
#endif
}

void led_strip_set_brightness(void *handle, uint8_t brightness) {
#if defined(DOTSTAR) || defined(HUB75)
  asStrip(handle)->setBrightness(brightness);
#else
  // T5.3: WS2812 brightness is handled in led_strip_set_pixel() or
  // by scaling the color values before storing.
  (void)handle;
  (void)brightness;
#endif
}

uint32_t led_strip_color(uint8_t r, uint8_t g, uint8_t b) {
#if defined(DOTSTAR) || defined(HUB75)
  return Strip::Color(r, g, b);
#else
  return (r << 16) | (g << 8) | b;
#endif
}

uint32_t led_strip_color_hsv(uint16_t hue, uint8_t sat, uint8_t val) {
#if defined(DOTSTAR) || defined(HUB75)
  return Strip::ColorHSV(hue, sat, val);
#else
  // Simple HSV to RGB conversion (no gamma correction).
  float h = hue / 65535.0f;
  float s = sat / 255.0f;
  float v = val / 255.0f;
  
  int i = (int)(h * 6.0f);
  float f = h * 6.0f - i;
  float p = v * (1.0f - s);
  float q = v * (1.0f - f * s);
  float t = v * (1.0f - (1.0f - f) * s);
  
  uint8_t r, g, b;
  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    default: r = 0; g = 0; b = 0; break;
  }
  
  return ((uint8_t)(r * 255.0f) << 16) | ((uint8_t)(g * 255.0f) << 8) | (uint8_t)(b * 255.0f);
#endif
}

uint32_t led_strip_gamma32(uint32_t color) {
#if defined(HUB75)
  (void)color;
  return 0;
#else
#if defined(DOTSTAR) || defined(HUB75)
  return Strip::gamma32(color);
#else
  // T5.3: WS2812 uses the pure C ws2812_rmt driver.
  // Gamma correction not applied (deferred to T5.x).
  return color;
#endif
#endif
}

uint16_t led_strip_color565(void *handle, uint8_t r, uint8_t g, uint8_t b) {
#if defined(HUB75)
  return asStrip(handle)->color565(r, g, b);
#else
  (void)handle;
  (void)r;
  (void)g;
  (void)b;
  return 0;
#endif
}

void led_strip_draw_pixel(void *handle, int x, int y, uint16_t color565) {
#if defined(HUB75)
  asStrip(handle)->drawPixel(x, y, color565);
#else
  (void)handle;
  (void)x;
  (void)y;
  (void)color565;
#endif
}

uint8_t *led_strip_pixels(void *handle) {
#if defined(DOTSTAR) || defined(HUB75)
  return asStrip(handle)->getPixels();
#else
  // T5.3: WS2812 uses the pure C ws2812_rmt driver.
  // The pixel buffer is the handle itself.
  return (uint8_t *)handle;
#endif
}

uint16_t led_strip_num_pixels(void *handle) {
#if defined(DOTSTAR) || defined(HUB75)
  return asStrip(handle)->numPixels();
#else
  (void)handle;
  return LED_COUNT;
#endif
}

} // extern "C"