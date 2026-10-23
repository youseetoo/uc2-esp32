// led_strip_c.cpp — C++ side of the led_strip_c.h shim (T4.1).
//
// The ONLY file in uc2_led that #includes Adafruit display headers. The strip
// variant is selected at build time: DOTSTAR (Adafruit_DotStar), HUB75
// (Adafruit_Protomatter), or NeoPixel (default). Host tests compile the
// NeoPixel branch against test/unity/stubs/Adafruit_NeoPixel.h.
#include "led_strip_c.h"
#include "PinConfig.h"

#define LED_COUNT (pinConfig.MATRIX_W * pinConfig.MATRIX_H)

#if defined(DOTSTAR)
#include "Adafruit_DotStar.h"
#elif defined(HUB75)
#include "Adafruit_Protomatter.h"
#else
#include "Adafruit_NeoPixel.h"
#if defined(USE_FASTACCEL)
#include "Ws2812Rmt.h"
#include <cstdlib>
#include <string>
#endif
#endif

namespace {

#if defined(DOTSTAR)
using Strip = Adafruit_DotStar;
#elif defined(HUB75)
using Strip = Adafruit_Protomatter;
#else
using Strip = Adafruit_NeoPixel;
#endif

Strip *asStrip(void *handle) {
  return static_cast<Strip *>(handle);
}

#if !defined(DOTSTAR) && !defined(HUB75) && defined(USE_FASTACCEL)
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
#endif

} // namespace

extern "C" {

void *led_strip_create(void) {
#if defined(DOTSTAR)
  Strip *strip =
      new Strip(LED_COUNT, pinConfig.LED_PIN, pinConfig.LED_CLK, DOTSTAR_BGR);
#elif defined(HUB75)
  Strip *strip = new Strip(
      pinConfig.MATRIX_W, pinConfig.HUB75_BIT_DEPTH, 1,
      const_cast<uint8_t *>(pinConfig.HUB75_RGB_PINS), 5,
      const_cast<uint8_t *>(pinConfig.HUB75_ADDR_PINS), pinConfig.HUB75_CLK,
      pinConfig.HUB75_LAT, pinConfig.HUB75_OE, true);
#else
  Strip *strip = new Strip(LED_COUNT, pinConfig.LED_PIN, NEO_GRB + NEO_KHZ800);
#endif
  return static_cast<void *>(strip);
}

bool led_strip_begin(void *handle) {
  Strip *strip = asStrip(handle);
#if defined(HUB75)
  return strip->begin() == 0;
#elif !defined(DOTSTAR) && defined(USE_FASTACCEL)
  return ws2812_rmt_begin(pinConfig.LED_PIN, rmtChannelFor(pinConfig.LED_PIN));
#else
  strip->begin();
  return true;
#endif
}

void led_strip_show(void *handle) {
  Strip *strip = asStrip(handle);
#if defined(DOTSTAR) || defined(HUB75)
  strip->show();
#elif defined(USE_FASTACCEL)
  ws2812_rmt_show(strip->getPixels(),
                  static_cast<uint32_t>(strip->numPixels()) * 3);
#else
  strip->show();
#endif
}

void led_strip_set_pixel(void *handle, uint16_t index, uint32_t color) {
  asStrip(handle)->setPixelColor(index, color);
}

void led_strip_clear(void *handle) {
#if defined(HUB75)
  asStrip(handle)->fillScreen(0);
#else
  asStrip(handle)->clear();
#endif
}

void led_strip_fill(void *handle, uint32_t color) {
#if defined(HUB75)
  asStrip(handle)->fillScreen(color);
#else
  asStrip(handle)->clear();
  asStrip(handle)->fill(color);
#endif
}

void led_strip_set_brightness(void *handle, uint8_t brightness) {
  asStrip(handle)->setBrightness(brightness);
}

uint32_t led_strip_color(uint8_t r, uint8_t g, uint8_t b) {
  return Strip::Color(r, g, b);
}

uint32_t led_strip_color_hsv(uint16_t hue, uint8_t sat, uint8_t val) {
  return Strip::ColorHSV(hue, sat, val);
}

uint32_t led_strip_gamma32(uint32_t color) {
#if defined(HUB75)
  (void)color;
  return 0;
#else
  return Strip::gamma32(color);
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
  return asStrip(handle)->getPixels();
}

uint16_t led_strip_num_pixels(void *handle) {
  return asStrip(handle)->numPixels();
}

} // extern "C"
