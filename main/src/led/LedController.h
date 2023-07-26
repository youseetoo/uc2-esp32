#pragma once
#include "Adafruit_NeoPixel.h"
#include "../../Module.h"

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



class LedController : public Module
{
private:
    // We use the strip instead of the matrix to ensure different dimensions; Convesion of the pattern has to be done on the cliet side!
    Adafruit_NeoPixel *matrix;
    bool DEBUG = false;
    bool isBusy;
    bool isOn = false;

    int NLED4x4 = 16;
    int NLED8x8 = 64;

    int LED_PATTERN_DPC_TOP_8x8[64] = {1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1,
                                       1, 1, 1, 1, 1, 1, 1, 1,
                                       0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0,
                                       0, 0, 0, 0, 0, 0, 0, 0};

    int LED_PATTERN_DPC_LEFT_8x8[64] = {1, 1, 1, 1, 0, 0, 0, 0,
                                        0, 0, 0, 0, 1, 1, 1, 1,
                                        1, 1, 1, 1, 0, 0, 0, 0,
                                        0, 0, 0, 0, 1, 1, 1, 1,
                                        1, 1, 1, 1, 0, 0, 0, 0,
                                        0, 0, 0, 0, 1, 1, 1, 1,
                                        1, 1, 1, 1, 0, 0, 0, 0,
                                        0, 0, 0, 0, 1, 1, 1, 1};

    int LED_PATTERN_DPC_TOP_4x4[16] = {1, 1, 1, 1,
                                       1, 1, 1, 1,
                                       0, 0, 0, 0,
                                       0, 0, 0, 0};

    int LED_PATTERN_DPC_LEFT_4x4[16] = {1, 1, 0, 0,
                                        0, 0, 1, 1,
                                        1, 1, 0, 0,
                                        0, 0, 1, 1};
    void set_led_RGB(u_int8_t iLed, u_int8_t R, u_int8_t G, u_int8_t B);
    void set_left(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B);
    void set_right(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B);
    void set_top(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B);
    void set_bottom(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B);

public:

    LedController();
    ~LedController();
    bool TurnedOn();
    void setup() override;
    void loop() override;
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
    int act(DynamicJsonDocument  ob) override;
    /*{
  "led": {
    "ledArrNum": 64,
    "ledArrPin": 27
  }
}
    */
    DynamicJsonDocument get(DynamicJsonDocument  ob) override;
    void set_all(u_int8_t R, u_int8_t G, u_int8_t B);
    void set_center(u_int8_t R, u_int8_t G, u_int8_t B);
};
//extern LedController led;
