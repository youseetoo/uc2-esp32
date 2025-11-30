#include <PinConfig.h>
#include "cJSON.h"
#include "driver/gpio.h"
#include "soc/gpio_sig_map.h"  // Provides signal definitions
#include "soc/io_mux_reg.h"
#pragma once
#ifdef HUB75
  #include <Adafruit_Protomatter.h>
#else
  #ifdef DOTSTAR
    #include <Adafruit_DotStar.h>
  #else
    #include <Adafruit_NeoPixel.h>
  #endif
#endif




enum LedForStatus : uint8_t
{
    warn=0,
    error, // red = 1
    idle,
    success,
    busy,
    rainbow,
    unknown, 
    on_, // white = 6
    off_, // black = 7
};

enum LedModes : uint8_t
{
    array = 0,
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
  STATUS,
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

};
#pragma pack(pop)




namespace LedController
{
    // We use the strip instead of the matrix to ensure different dimensions; Convesion of the pattern has to be done on the cliet side!
    #ifdef  DOTSTAR
    static Adafruit_DotStar *matrix;
    #elif defined(HUB75)
    static Adafruit_Protomatter *matrix;
    #else
    static Adafruit_NeoPixel *matrix;
    #endif

    static bool isDEBUG = false;
    static bool isOn = false;

    // for in-loop interaction 
    static int currentLedForStatus = LedForStatus::idle;
	static uint8_t brightnessLoop = 0;    // Tracks current fade brightnessLoop
    static int fadeDirection = 1;     // Fade up (+1) or down (-1)
	
    // Adjust for your NeoPixel matrix:
    static const uint16_t LED_COUNT = pinConfig.MATRIX_W * pinConfig.MATRIX_H;

    void setPixelXY(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    uint16_t xyToIndex(int x, int y);
    void turnOff();
    void fillAll(uint8_t r, uint8_t g, uint8_t b);
    void setSingle(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    void fillHalves(const char *region, uint8_t r, uint8_t g, uint8_t b);
    void fillHalvesRingSegments(const char *region, uint8_t r, uint8_t g, uint8_t b);
    void drawRings(uint8_t radius, uint8_t rVal, uint8_t gVal, uint8_t bVal);
    void drawIlluminationRings(uint8_t ring_id, uint8_t r, uint8_t g, uint8_t b);
    void drawIlluminationRingSegment(uint8_t ring_id, const char* region, uint8_t r, uint8_t g, uint8_t b);
    void drawCircle(uint8_t radius, uint8_t rVal, uint8_t gVal, uint8_t bVal);
    bool parseLedCommand(cJSON *root, LedCommand &cmd);
    void execLedCommand(const LedCommand &cmd);
    void cross_changed_event(int pressed);
    void circle_changed_event(int pressed);
    uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);


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
