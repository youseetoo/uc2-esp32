#include <PinConfig.h>
#pragma once
#include "Adafruit_NeoPixel.h"
#include "cJSON.h"


enum LedModes
{
    array,
    full,
    single,
    off,
    left,
    right,
    top,
    bottom,
    multi
};



namespace LedController
{
    // We use the strip instead of the matrix to ensure different dimensions; Convesion of the pattern has to be done on the cliet side!
    static Adafruit_NeoPixel *matrix;
    static bool isDEBUG = false;
    static bool isOn = false;

    static int NLED4x4 = 16;
    static int NLED8x8 = 64;

    static int LED_PATTERN_DPC_TOP_8x8[64] = {1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1,
                                       0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0};

    static int LED_PATTERN_DPC_LEFT_8x8[64] = {1, 1, 1, 1, 0, 0, 0, 0,
                                        0, 0, 0, 0, 1, 1, 1, 1,
                                        1, 1, 1, 1, 0, 0, 0, 0,
                                        0, 0, 0, 0, 1, 1, 1, 1,
                                        1, 1, 1, 1, 0, 0, 0, 0,
                                        0, 0, 0, 0, 1, 1, 1, 1,
                                        1, 1, 1, 1, 0, 0, 0, 0,
                                        0, 0, 0, 0, 1, 1, 1, 1};

    static int LED_PATTERN_DPC_TOP_4x4[16] = {1, 1, 1, 1,
                                       1, 1, 1, 1,
                                       0, 0, 0, 0,
                                       0, 0, 0, 0};

    static int LED_PATTERN_DPC_LEFT_4x4[16] = {1, 1, 0, 0,
                                        0, 0, 1, 1,
                                        1, 1, 0, 0,
                                        0, 0, 1, 1};
    void set_led_RGB(u_int8_t iLed, u_int8_t R, u_int8_t G, u_int8_t B);
    void set_left(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B);
    void set_right(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B);
    void set_top(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B);
    void set_bottom(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B);

    bool TurnedOn();
    void setup();
    /*
    {
    "led": {
        "LEDArrMode": 1,
        "led_array": [
        {
            "b": 0,
            "g": 0,
            "id": 0,
            "r": 0
        }
        ]
    }
    }
    */
    int act(cJSON * ob);
    /*{
  "led": {
    "ledArrNum": 64,
    "ledArrPin": 27
  }
}
    */
    cJSON * get(cJSON *  ob);
    void loop();
    void set_all(u_int8_t R, u_int8_t G, u_int8_t B);
    void set_center(u_int8_t R, u_int8_t G, u_int8_t B);
    void cross_changed_event(uint8_t pressed);
    void circle_changed_event(uint8_t pressed);
};
