#include "LedController.h"
#include "../../pindef.h"

namespace RestApi
{
	void Led_act()
	{
		serialize(moduleController.get(AvailableModules::led)->act(deserialize()));
	}

	void Led_get()
	{
		serialize(moduleController.get(AvailableModules::led)->get(deserialize()));

	}

	void Led_set()
	{
		serialize(moduleController.get(AvailableModules::led)->set(deserialize()));
	}
}

LedController::LedController() : Module() { log_i("ctor"); }
LedController::~LedController() { log_i("~ctor"); }

void LedController::setup()
{
	// get led config from preferences
	ledconfig = Config::getLedPins();

	// load default values if not set
	if (not ledconfig->ledPin)
		ledconfig->ledPin = PIN_DEF_LED;
	if (not ledconfig->ledCount)
		ledconfig->ledCount = PIN_DEF_LED_NUM;

	// LED Matrix
	matrix = new Adafruit_NeoPixel(ledconfig->ledCount, ledconfig->ledPin, NEO_GRB + NEO_KHZ800);
	log_i("LED_ARRAY_PIN: %i and LED_NUM: %i", ledconfig->ledPin, ledconfig->ledCount);
	//log_i("setup matrix is null:%s", (bool)matrix == nullptr);
	
	// initialize LED Strip
	matrix->begin();
	matrix->setBrightness(255);

	// either set the default color or turn off the leds
	if (!isOn)
		set_all(0, 0, 0);
	else
		set_all(255, 255, 255);
	matrix->show(); //  Update strip to match

	// write out updated config to Preferences
	Config::setLedPins(ledconfig);
}

void LedController::loop()
{
}

bool LedController::TurnedOn()
{
	return isOn;
}

// Custom function accessible by the API
int LedController::act(DynamicJsonDocument ob)
{
	
		/*
		Mode 0: array,
		Mode 1: full,
		Mode 2: single,
		Mode 3: off,
		Mode 4: left,
		Mode 5: right,
		Mode 6: top,
		Mode 7: bottom,
		Mode 8: multi
		*/
	if (ob.containsKey(keyLed))
	{
		LedModes LEDArrMode = static_cast<LedModes>(ob[keyLed][keyLEDArrMode]); // "array", "full", "single", "off", "left", "right", "top", "bottom",
		// individual pattern gets adressed
		if (LEDArrMode == LedModes::array || LEDArrMode == LedModes::multi)
		{
			for (int i = 0; i < ob[keyLed][key_led_array].size(); i++)
			{
				set_led_RGB(
					ob[keyLed][key_led_array][i][keyid],
					ob[keyLed][key_led_array][i][keyRed],
					ob[keyLed][key_led_array][i][keyGreen],
					ob[keyLed][key_led_array][i][keyBlue]);
			}
			matrix->show(); //  Update strip to match
			ob.clear();
			ob[key_return] = LEDArrMode;
		}
		// only if a single led will be updated, all others stay the same
		else if (LEDArrMode == LedModes::single)
		{
			set_led_RGB(
				ob[keyLed][key_led_array][0][keyid],
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
				matrix->show(); //  Update strip to match
				ob[key_return] = LEDArrMode;
					}
		// turn on all LEDs
		else if (LEDArrMode == LedModes::full)
		{
			u_int8_t r = ob[keyLed][key_led_array][0][keyRed];
			u_int8_t g = ob[keyLed][key_led_array][0][keyGreen];
			u_int8_t b = ob[keyLed][key_led_array][0][keyBlue];
			isOn = r == 0 && g == 0 && b == 0 ? false : true;
			set_all(r, g, b);
			matrix->show(); //  Update strip to match
			matrix->show(); //  Update strip to match
			ob[key_return] = LEDArrMode;
		}
		// turn off all LEDs
		else if (LEDArrMode == LedModes::left)
		{
			set_left(
				ledconfig->ledCount,
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
				matrix->show(); //  Update strip to match
				ob[key_return] = LEDArrMode;
		}
		// turn off all LEDs
		else if (LEDArrMode == LedModes::right)
		{
			set_right(
				ledconfig->ledCount,
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
				matrix->show(); //  Update strip to match
				ob[key_return] = LEDArrMode;
		}
		// turn off all LEDs
		else if (LEDArrMode == LedModes::top)
		{
			set_top(
				ledconfig->ledCount,
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
				matrix->show(); //  Update strip to match
				ob[key_return] = LEDArrMode;
		}
		// turn off all LEDs
		else if (LEDArrMode == LedModes::bottom)
		{
			set_bottom(
				ledconfig->ledCount,
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
				matrix->show(); //  Update strip to match
				ob[key_return] = LEDArrMode;
		}
		else if (LEDArrMode == LedModes::off)
		{
			matrix->clear();
			ob.clear();
			ob[key_return] = LEDArrMode;
		}
	}
	else
	{
		ob.clear();
		ob[key_return] = -1;
		log_i("failed to parse json. required keys are led_array,LEDArrMode");
	}
	return 1;
}

//{"led":{"LEDArrMode":1,"led_array":[{"id":0,"blue":"128","red":"128","green":"128"}]}}
//{"task" : "/ledarr_act", "led":{"LEDArrMode":1,"led_array":[{"id":0,"blue":"0","red":"0","green":"0"}]}}
int LedController::set(DynamicJsonDocument ob)
{
	if (ob.containsKey(keyLed))
	{
		if (ob[keyLed].containsKey(keyLEDPin))
			ledconfig->ledPin = ob[keyLed][keyLEDPin];
		if (ob[keyLed].containsKey(keyLEDCount))
			ledconfig->ledCount = ob[keyLed][keyLEDCount];
		log_i("led pin:%i count:%i", ledconfig->ledPin, ledconfig->ledCount);
		Config::setLedPins(ledconfig);
		setup();
	}
	return 1;
}

// Custom function accessible by the API
DynamicJsonDocument LedController::get(DynamicJsonDocument ob)
{
	ob.clear();
	ob[keyLEDCount] = ledconfig->ledCount;
	ob[keyLEDPin] = ledconfig->ledPin;
	ob[keyLEDArrMode].add(0);
	ob[keyLEDArrMode].add(1);
	ob[keyLEDArrMode].add(2);
	ob[keyLEDArrMode].add(3);
	ob[keyLEDArrMode].add(4);
	ob[keyLEDArrMode].add(5);
	ob[keyLEDArrMode].add(6);
	ob[keyLEDArrMode].add(7);
	ob[key_led_isOn] = isOn;
	return ob;
}

/***************************************************************************************************/
/*******************************************  LED Array  *******************************************/
/***************************************************************************************************/
/*******************************FROM OCTOPI ********************************************************/

void LedController::set_led_RGB(u_int8_t iLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	matrix->setPixelColor(iLed, matrix->Color(R, G, B)); //  Set pixel's color (in RAM)
}

void LedController::set_all(u_int8_t R, u_int8_t G, u_int8_t B)
{
	for (int i = 0; i < matrix->numPixels(); i++)
	{
		set_led_RGB(i, R, G, B);
	}
}

void LedController::set_left(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	if (NLed == NLED4x4)
	{
		for (int i = 0; i < (NLED4x4); i++)
		{
			set_led_RGB(i, LED_PATTERN_DPC_LEFT_4x4[i] * R, LED_PATTERN_DPC_LEFT_4x4[i] * G, LED_PATTERN_DPC_LEFT_4x4[i] * B);
		}
	}
	if (NLed == NLED8x8)
	{
		for (int i = 0; i < (NLED8x8); i++)
		{
			set_led_RGB(i, LED_PATTERN_DPC_LEFT_8x8[i] * R, LED_PATTERN_DPC_LEFT_8x8[i] * G, LED_PATTERN_DPC_LEFT_8x8[i] * B);
		}
	}
}

void LedController::set_right(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	if (NLed == NLED4x4)
	{
		for (int i = 0; i < (NLED4x4); i++)
		{
			set_led_RGB(i, (1 - LED_PATTERN_DPC_LEFT_4x4[i]) * R, (1 - LED_PATTERN_DPC_LEFT_4x4[i]) * G, (1 - LED_PATTERN_DPC_LEFT_4x4[i]) * B);
		}
	}
	if (NLed == NLED8x8)
	{
		for (int i = 0; i < (NLED8x8); i++)
		{
			set_led_RGB(i, (1 - LED_PATTERN_DPC_LEFT_8x8[i]) * R, (1 - LED_PATTERN_DPC_LEFT_8x8[i]) * G, (1 - LED_PATTERN_DPC_LEFT_8x8[i]) * B);
		}
	}
}

void LedController::set_top(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	if (NLed == NLED4x4)
	{
		for (int i = 0; i < (NLED4x4); i++)
		{
			set_led_RGB(i, (LED_PATTERN_DPC_TOP_4x4[i]) * R, (LED_PATTERN_DPC_TOP_4x4[i]) * G, (LED_PATTERN_DPC_TOP_4x4[i]) * B);
		}
	}
	if (NLed == NLED8x8)
	{
		for (int i = 0; i < (NLED8x8); i++)
		{
			set_led_RGB(i, (LED_PATTERN_DPC_TOP_8x8[i]) * R, (LED_PATTERN_DPC_TOP_8x8[i]) * G, (LED_PATTERN_DPC_TOP_8x8[i]) * B);
		}
	}
}

void LedController::set_bottom(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	if (NLed == NLED4x4)
	{
		for (int i = 0; i < (NLED4x4); i++)
		{
			set_led_RGB(i, (1 - LED_PATTERN_DPC_TOP_4x4[i]) * R, (1 - LED_PATTERN_DPC_TOP_4x4[i]) * G, (1 - LED_PATTERN_DPC_TOP_4x4[i]) * B);
		}
	}
	if (NLed == NLED8x8)
	{
		for (int i = 0; i < (NLED8x8); i++)
		{
			set_led_RGB(i, (1 - LED_PATTERN_DPC_TOP_8x8[i]) * R, (1 - LED_PATTERN_DPC_TOP_8x8[i]) * G, (1 - LED_PATTERN_DPC_TOP_8x8[i]) * B);
		}
	}
}

void LedController::set_center(u_int8_t R, u_int8_t G, u_int8_t B)
{
	/*
	matrix.fillScreen(matrix.Color(0, 0, 0));
	matrix.drawPixel(4, 4, matrix.Color(R,   G,   B));
	matrix.show();
	*/
}
// LedController led;
