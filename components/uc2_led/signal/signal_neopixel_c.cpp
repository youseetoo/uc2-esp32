// T4.1: C++ implementation of the signal_neopixel_c.h shim — forwards the
// C calls from the pure-C SignalController.c to the C++ Adafruit_NeoPixel
// class (same pattern as uc2_motor/motor/focusmotor_qid_c.cpp for FocusMotor).
#include "signal_neopixel_c.h"
#include "Adafruit_NeoPixel.h"

extern "C" void *signal_strip_create(uint16_t numPixels, int pin)
{
    // Only configuration used by SignalController (FRAME status pixel).
    Adafruit_NeoPixel *strip = new Adafruit_NeoPixel(numPixels, pin, NEO_GRB + NEO_KHZ800);
    return static_cast<void *>(strip);
}

extern "C" void signal_strip_destroy(void *strip)
{
    delete static_cast<Adafruit_NeoPixel *>(strip);
}

extern "C" void signal_strip_begin(void *strip)
{
    static_cast<Adafruit_NeoPixel *>(strip)->begin();
}

extern "C" void signal_strip_set_brightness(void *strip, uint8_t b)
{
    static_cast<Adafruit_NeoPixel *>(strip)->setBrightness(b);
}

extern "C" void signal_strip_clear(void *strip)
{
    static_cast<Adafruit_NeoPixel *>(strip)->clear();
}

extern "C" void signal_strip_show(void *strip)
{
    static_cast<Adafruit_NeoPixel *>(strip)->show();
}

extern "C" uint8_t signal_strip_gamma8(void *strip, uint8_t v)
{
    return static_cast<Adafruit_NeoPixel *>(strip)->gamma8(v);
}

extern "C" uint32_t signal_strip_color(void *strip, uint8_t r, uint8_t g, uint8_t b)
{
    return static_cast<Adafruit_NeoPixel *>(strip)->Color(r, g, b);
}

extern "C" void signal_strip_set_pixel(void *strip, uint16_t idx, uint32_t c)
{
    static_cast<Adafruit_NeoPixel *>(strip)->setPixelColor(idx, c);
}
