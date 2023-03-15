#include "LedController.h"

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
}

LedController::LedController() : Module() { log_i("ctor"); }
LedController::~LedController() { log_i("~ctor"); }

void LedController::setup()
{
	// LED Matrix
	matrix = new Adafruit_NeoPixel(pinConfig.LED_COUNT, pinConfig.LED_PIN, NEO_GRB + NEO_KHZ800);
	//log_i("setup matrix is null:%s", matrix == nullptr);
	log_i("LED_ARRAY_PIN: %i", pinConfig.LED_PIN);
	matrix->begin();
	matrix->setBrightness(255);
	// test led array
	set_all(100,100,100);
	delay(1);
	set_all(0,0,0);

	if (!isOn)
		set_all(0, 0, 0);
	else
		set_all(255, 255, 255);
	matrix->show(); //  Update strip to match
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
	serializeJsonPretty(ob, Serial);
	if (ob.containsKey(keyLed))
	{
		LedModes LEDArrMode = static_cast<LedModes>(ob[keyLed][keyLEDArrMode]); // "array", "full", "single", "off", "left", "right", "top", "bottom",
		// individual pattern gets adressed
		// PYTHON: send_LEDMatrix_array(self, led_pattern, timeout=1)
		if (LEDArrMode == LedModes::array || LEDArrMode == LedModes::multi)
		{
			log_d("LED: array/multi");
			for (int i = 0; i < ob[keyLed][key_led_array].size(); i++)
			{
				set_led_RGB(
					ob[keyLed][key_led_array][i][keyid],
					ob[keyLed][key_led_array][i][keyRed],
					ob[keyLed][key_led_array][i][keyGreen],
					ob[keyLed][key_led_array][i][keyBlue]);
			}
			matrix->show(); //  Update strip to match
		}
		// only if a single led will be updated, all others stay the same
		// {"task": "/ledarr_act", "led": {"LEDArrMode": 2, "led_array": [{"id": 0, "r": 255, "g": 255, "b": 255}]}}
		else if (LEDArrMode == LedModes::single)
		{
			log_d("LED: single");
			set_led_RGB(
				ob[keyLed][key_led_array][0][keyid],
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
			matrix->show(); //  Update strip to match
		}
		// turn on all LEDs
		// PYTHON: send_LEDMatrix_full(self, intensity = (255,255,255),timeout=1)
		else if (LEDArrMode == LedModes::full)
		{
			log_d("LED: full");
			u_int8_t r = ob[keyLed][key_led_array][0][keyRed];
			u_int8_t g = ob[keyLed][key_led_array][0][keyGreen];
			u_int8_t b = ob[keyLed][key_led_array][0][keyBlue];
			isOn = r == 0 && g == 0 && b == 0 ? false : true;
			set_all(r, g, b);
		}
		// set left
		else if (LEDArrMode == LedModes::left)
		{
			log_d("LED: left");
			set_left(
				pinConfig.LED_COUNT,
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
		}
		// set right
		else if (LEDArrMode == LedModes::right)
		{
			log_d("LED: right");
			set_right(
				pinConfig.LED_COUNT,
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
		}
		// set top
		else if (LEDArrMode == LedModes::top)
		{
			log_d("LED: top");
			set_top(
				pinConfig.LED_COUNT,
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
		}
		// set bottom
		else if (LEDArrMode == LedModes::bottom)
		{
			log_d("LED: bottom");
			set_bottom(
				pinConfig.LED_COUNT,
				ob[keyLed][key_led_array][0][keyRed],
				ob[keyLed][key_led_array][0][keyGreen],
				ob[keyLed][key_led_array][0][keyBlue]);
		}
		else if (LEDArrMode == LedModes::off)
		{
			log_d("LED: all off");
			matrix->clear();
			matrix->show(); //  Update strip to match
		}
	}
	else
	{
		log_i("failed to parse json. required keys are led_array,LEDArrMode");
	}
	return 1;
}

// Custom function accessible by the API
DynamicJsonDocument LedController::get(DynamicJsonDocument ob)
{
	ob.clear();
	ob[keyLEDCount] = pinConfig.LED_COUNT;
	ob[keyLEDPin] = pinConfig.LED_PIN;
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


void LedController::set_led_RGB(u_int8_t iLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	//log_d("setting led %i, color %i, %i, %i", iLed, R, G, B);
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
