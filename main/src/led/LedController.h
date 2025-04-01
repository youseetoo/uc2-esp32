#include <PinConfig.h>
#pragma once
#ifdef DOTSTAR
#include <Adafruit_DotStar.h>
#else
#include <Adafruit_NeoPixel.h>
#endif
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

enum class LedMode : uint8_t
{
	OFF = 0,
	FILL,
	SINGLE,
	HALVES,
	RINGS,
	CIRCLE,
	ARRAY, // for multiple arbitrary pixels
	UNKNOWN
};

#pragma pack(push, 1)
struct LedCommand
{
	uint16_t qid;	// user-assigned ID
	LedMode mode;	// see enum above
	uint8_t r;		// color R
	uint8_t g;		// color G
	uint8_t b;		// color B
	uint8_t radius; // used for RINGS or CIRCLE
	char region[8]; // "left","right","top","bottom" (for HALVES)
	// For SINGLE pixel
	uint16_t ledIndex;
	// For an 'ARRAY' of pixel updates, your code can parse them from cJSON if desired
};
#pragma pack(pop)




namespace LedController
{
    // We use the strip instead of the matrix to ensure different dimensions; Convesion of the pattern has to be done on the cliet side!
    #ifdef  DOTSTAR
    static Adafruit_DotStar *matrix;
    #else
    static Adafruit_NeoPixel *matrix;
    #endif

    static bool isDEBUG = false;
    static bool isOn = false;

    // Adjust for your NeoPixel matrix:
    static const uint16_t LED_COUNT = pinConfig.MATRIX_W * pinConfig.MATRIX_H;

    void setPixelXY(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    uint16_t xyToIndex(int x, int y);
    void turnOff();
    void fillAll(uint8_t r, uint8_t g, uint8_t b);
    void setSingle(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    void fillHalves(const char *region, uint8_t r, uint8_t g, uint8_t b);
    void drawRings(uint8_t radius, uint8_t rVal, uint8_t gVal, uint8_t bVal);
    void drawCircle(uint8_t radius, uint8_t rVal, uint8_t gVal, uint8_t bVal);
    bool parseLedCommand(cJSON *root, LedCommand &cmd);
    void execLedCommand(const LedCommand &cmd);
    void cross_changed_event(int pressed);
    void circle_changed_event(int pressed);


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

};
