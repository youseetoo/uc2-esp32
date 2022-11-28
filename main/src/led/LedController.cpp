#include "../../config.h"
#include "LedController.h"
#include "../../pindef.h"

namespace RestApi
{
	void Led_act()
	{
		deserialize();
		moduleController.get(AvailableModules::led)->act();
		serialize();
	}

	void Led_get()
	{
		deserialize();
		moduleController.get(AvailableModules::led)->get();
		serialize();
	}

	void Led_set()
	{
		deserialize();
		moduleController.get(AvailableModules::led)->set();
		serialize();
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
	log_i("setup matrix is null:%s", boolToChar(matrix == nullptr));
	log_i("LED_ARRAY_PIN: %i", ledconfig->ledPin);
	matrix->begin();
	matrix->setBrightness(255);
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

// Custom function accessible by the API
void LedController::act()
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

	// get json document through serial
	DynamicJsonDocument *jDoc = WifiController::getJDoc();

	if (WifiController::getJDoc()->containsKey(keyLed))
	{
		LedModes LEDArrMode = static_cast<LedModes>((*jDoc)[keyLed][keyLEDArrMode]); // "array", "full", "single", "off", "left", "right", "top", "bottom",

		// individual pattern gets adressed
		if (LEDArrMode == LedModes::array || LEDArrMode == LedModes::multi)
		{
			for (int i = 0; i < (*jDoc)[keyLed][key_led_array].size(); i++)
			{
				set_led_RGB(
					(*jDoc)[keyLed][key_led_array][i][keyid],
					(*jDoc)[keyLed][key_led_array][i][keyRed],
					(*jDoc)[keyLed][key_led_array][i][keyGreen],
					(*jDoc)[keyLed][key_led_array][i][keyBlue]);
			}
			jDoc->clear();
			(*jDoc)[key_return] = LEDArrMode;
		}
		// only if a single led will be updated, all others stay the same
		else if (LEDArrMode == LedModes::single)
		{
			set_led_RGB(
				(*jDoc)[keyLed][key_led_array][0][keyid],
				(*jDoc)[keyLed][key_led_array][0][keyRed],
				(*jDoc)[keyLed][key_led_array][0][keyGreen],
				(*jDoc)[keyLed][key_led_array][0][keyBlue]);

			jDoc->clear();
			(*jDoc)[key_return] = LEDArrMode;
		}
		// turn on all LEDs
		else if (LEDArrMode == LedModes::full)
		{
			u_int8_t r = (*jDoc)[keyLed][key_led_array][0][keyRed];
			u_int8_t g = (*jDoc)[keyLed][key_led_array][0][keyGreen];
			u_int8_t b = (*jDoc)[keyLed][key_led_array][0][keyBlue];
			isOn = r == 0 && g == 0 && b == 0 ? false : true;
			set_all(r, g, b);

			jDoc->clear();
			(*jDoc)[key_return] = LEDArrMode;
		}
		// turn off all LEDs
		else if (LEDArrMode == LedModes::left)
		{
			set_left(
				ledconfig->ledCount,
				(*jDoc)[keyLed][key_led_array][0][keyRed],
				(*jDoc)[keyLed][key_led_array][0][keyGreen],
				(*jDoc)[keyLed][key_led_array][0][keyBlue]);
			jDoc->clear();
			(*jDoc)[key_return] = LEDArrMode;
		}
		// turn off all LEDs
		else if (LEDArrMode == LedModes::right)
		{
			set_right(
				ledconfig->ledCount,
				(*jDoc)[keyLed][key_led_array][0][keyRed],
				(*jDoc)[keyLed][key_led_array][0][keyGreen],
				(*jDoc)[keyLed][key_led_array][0][keyBlue]);
			jDoc->clear();
			(*jDoc)[key_return] = LEDArrMode;
		}
		// turn off all LEDs
		else if (LEDArrMode == LedModes::top)
		{
			set_top(
				ledconfig->ledCount,
				(*jDoc)[keyLed][key_led_array][0][keyRed],
				(*jDoc)[keyLed][key_led_array][0][keyGreen],
				(*jDoc)[keyLed][key_led_array][0][keyBlue]);
			jDoc->clear();
			(*jDoc)[key_return] = LEDArrMode;
		}
		// turn off all LEDs
		else if (LEDArrMode == LedModes::bottom)
		{
			set_bottom(
				ledconfig->ledCount,
				(*jDoc)[keyLed][key_led_array][0][keyRed],
				(*jDoc)[keyLed][key_led_array][0][keyGreen],
				(*jDoc)[keyLed][key_led_array][0][keyBlue]);
			jDoc->clear();
			(*jDoc)[key_return] = LEDArrMode;
		}
		else if (LEDArrMode == LedModes::off)
		{
			matrix->clear();
			jDoc->clear();
			(*jDoc)[key_return] = LEDArrMode;
		}
	}
	else
	{
		jDoc->clear();
		(*jDoc)[key_return] = -1;
		log_i("failed to parse json. required keys are led_array,LEDArrMode");
	}
}

//{"led":{"LEDArrMode":1,"led_array":[{"id":0,"blue":"128","red":"128","green":"128"}]}}
//{"task" : "/ledarr_act", "led":{"LEDArrMode":1,"led_array":[{"id":0,"blue":"0","red":"0","green":"0"}]}}
void LedController::set()
{
	if (WifiController::getJDoc()->containsKey(keyLed))
	{
		if ((*WifiController::getJDoc())[keyLed].containsKey(keyLEDPin))
			ledconfig->ledPin = (*WifiController::getJDoc())[keyLed][keyLEDPin];
		if ((*WifiController::getJDoc())[keyLed].containsKey(keyLEDCount))
			ledconfig->ledCount = (*WifiController::getJDoc())[keyLed][keyLEDCount];
		log_i("led pin:%i count:%i", ledconfig->ledPin, ledconfig->ledCount);
		Config::setLedPins(ledconfig);
		setup();
	}
	WifiController::getJDoc()->clear();
}

// Custom function accessible by the API
void LedController::get()
{
	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())[keyLEDCount] = ledconfig->ledCount;
	(*WifiController::getJDoc())[keyLEDPin] = ledconfig->ledPin;
	(*WifiController::getJDoc())[keyLEDArrMode].add(0);
	(*WifiController::getJDoc())[keyLEDArrMode].add(1);
	(*WifiController::getJDoc())[keyLEDArrMode].add(2);
	(*WifiController::getJDoc())[keyLEDArrMode].add(3);
	(*WifiController::getJDoc())[keyLEDArrMode].add(4);
	(*WifiController::getJDoc())[keyLEDArrMode].add(5);
	(*WifiController::getJDoc())[keyLEDArrMode].add(6);
	(*WifiController::getJDoc())[keyLEDArrMode].add(7);
	(*WifiController::getJDoc())[key_led_isOn] = isOn;
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
	matrix->show(); //  Update strip to match
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
	matrix->show(); //  Update strip to match
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
	matrix->show(); //  Update strip to match
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
	matrix->show(); //  Update strip to match
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
	matrix->show(); //  Update strip to match
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
