#pragma once
#include "LedController.h"
#include "JsonKeys.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "cJSON.h"
#include "PinConfig.h" // user-provided config, if needed

#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif


// --------------------------------------------------------------------------------
// 3) The LedController namespace
// --------------------------------------------------------------------------------
namespace LedController
{

	const char *TAG = "LedController";

	// ------------------------------------------------
	// HELPER: Convert (x, y) -> single index
	// ------------------------------------------------
	uint16_t xyToIndex(int x, int y)
	{
		// For top-left → bottom-right wiring
		return y * pinConfig.MATRIX_W + x;
	}

	// ------------------------------------------------
	// HELPER: Set one pixel in x/y coords
	// ------------------------------------------------
	void setPixelXY(int x, int y, uint8_t r, uint8_t g, uint8_t b)
	{
		if (x < 0 || x >= pinConfig.MATRIX_W)
			return;
		if (y < 0 || y >= pinConfig.MATRIX_H)
			return;
		uint16_t idx = xyToIndex(x, y);
		matrix->setPixelColor(idx, matrix->Color(r, g, b));
	}

	// ------------------------------------------------
	// 4) Setup: Initialize the matrix
	// ------------------------------------------------
	void setup()
	{

		#ifdef DOTSTAR
		matrix = new Adafruit_DotStar(LED_COUNT, pinConfig.LED_PIN, pinConfig.LED_CLK, DOTSTAR_BGR);
		#else
		matrix = new Adafruit_NeoPixel(LED_COUNT, pinConfig.LED_PIN, NEO_GRB + NEO_KHZ800);
		#endif
		matrix->begin();
		matrix->setBrightness(40); // moderate brightness
		matrix->clear();
		matrix->show();

		// test led array
		fillAll(10,10,10);
		delay(10);
		fillAll(0,0,0);
		matrix->show(); //  Update strip to match

		isOn = false;
	}

	// ------------------------------------------------
	// 5) "Off" all LEDs
	// ------------------------------------------------
	void turnOff()
	{
		matrix->clear();
		matrix->show();
		isOn = false;
	}

	// ------------------------------------------------
	// 6) Fill entire matrix with a single color
	// ------------------------------------------------
	void fillAll(uint8_t r, uint8_t g, uint8_t b)
	{
		for (uint16_t i = 0; i < LED_COUNT; i++)
		{
			matrix->setPixelColor(i, matrix->Color(r, g, b));
		}
		matrix->show();
		isOn = (r || g || b);
	}

	// ------------------------------------------------
	// 7) Light a SINGLE LED by index
	// ------------------------------------------------
	void setSingle(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
	{
		if (index < LED_COUNT)
		{
			matrix->clear();
			matrix->setPixelColor(index, matrix->Color(r, g, b));
			matrix->show();
			isOn = (r || g || b);
		}
	}

	// ------------------------------------------------
	// 8) Halves: fill left/right/top/bottom with color, rest dark
	// ------------------------------------------------
	void fillHalves(const char *region, uint8_t r, uint8_t g, uint8_t b)
	{
		matrix->clear();

		if (strcasecmp(region, "left") == 0)
		{
			for (int y = 0; y < pinConfig.MATRIX_H; y++)
			{
				for (int x = 0; x < pinConfig.MATRIX_W / 2; x++)
				{
					setPixelXY(x, y, r, g, b);
				}
			}
		}
		else if (strcasecmp(region, "right") == 0)
		{
			for (int y = 0; y < pinConfig.MATRIX_H; y++)
			{
				for (int x = pinConfig.MATRIX_W / 2; x < pinConfig.MATRIX_W; x++)
				{
					setPixelXY(x, y, r, g, b);
				}
			}
		}
		else if (strcasecmp(region, "top") == 0)
		{
			for (int y = 0; y < pinConfig.MATRIX_H / 2; y++)
			{
				for (int x = 0; x < pinConfig.MATRIX_W; x++)
				{
					setPixelXY(x, y, r, g, b);
				}
			}
		}
		else if (strcasecmp(region, "bottom") == 0)
		{
			for (int y = pinConfig.MATRIX_H / 2; y < pinConfig.MATRIX_H; y++)
			{
				for (int x = 0; x < pinConfig.MATRIX_W; x++)
				{
					setPixelXY(x, y, r, g, b);
				}
			}
		}
		// else unknown => remain dark

		matrix->show();
		isOn = (r || g || b);
	}

	// ------------------------------------------------
	// 9) RINGS: for demonstration, fill a big circle,
	//           then carve out the interior to simulate a ring
	//           or you can do multiple concentric rings, etc.
	// ------------------------------------------------
	void drawRings(uint8_t radius, uint8_t r, uint8_t g, uint8_t b)
	{
		// The simplest approach to get a ring:
		// 1) Fill circle of radius
		// 2) Fill circle of radius-1 in black
		// => That leaves a 1-pixel wide ring
		matrix->clear();

		// fill circle
		float cx = (pinConfig.MATRIX_W - 1) / 2.0;
		float cy = (pinConfig.MATRIX_H - 1) / 2.0;
		for (int y = 0; y < pinConfig.MATRIX_H; y++)
		{
			for (int x = 0; x < pinConfig.MATRIX_W; x++)
			{
				float dx = x - cx;
				float dy = y - cy;
				float dist = sqrtf(dx * dx + dy * dy);
				if (dist <= radius)
				{
					setPixelXY(x, y, r, g, b);
				}
			}
		}

		// carve out inside => smaller circle in black
		if (radius > 0)
		{
			for (int y = 0; y < pinConfig.MATRIX_H; y++)
			{
				for (int x = 0; x < pinConfig.MATRIX_W; x++)
				{
					float dx = x - cx;
					float dy = y - cy;
					float dist = sqrtf(dx * dx + dy * dy);
					if (dist <= (radius - 1))
					{
						setPixelXY(x, y, 0, 0, 0);
					}
				}
			}
		}

		matrix->show();
		isOn = (r || g || b);
	}

	// ------------------------------------------------
	// 10) Filled circle
	// ------------------------------------------------
	void drawCircle(uint8_t radius, uint8_t rVal, uint8_t gVal, uint8_t bVal)
	{
		matrix->clear();

		float cx = (pinConfig.MATRIX_W - 1) / 2.0;
		float cy = (pinConfig.MATRIX_H - 1) / 2.0;
		for (int y = 0; y < pinConfig.MATRIX_H; y++)
		{
			for (int x = 0; x < pinConfig.MATRIX_W; x++)
			{
				float dx = x - cx;
				float dy = y - cy;
				float dist = sqrtf(dx * dx + dy * dy);
				if (dist <= radius)
				{
					setPixelXY(x, y, rVal, gVal, bVal);
				}
			}
		}

		matrix->show();
		isOn = (rVal || gVal || bVal);
	}

	// ------------------------------------------------
	// 11) Parse cJSON into LedCommand struct
	// ------------------------------------------------
	bool parseLedCommand(cJSON *root, LedCommand &cmd)
	{
		// Expect a top-level "task" == "/ledarr_act" plus "qid",
		// then a nested "led" object with the actual LED parameters.
		// E.g.:
		// { "task": "/ledarr_act", "qid": 17, "led": { "LEDArrMode": 1, "action": "rings", "region": "left", "radius": 4, "r": 255, "g": 255, "b": 255, "ledIndex": 12, "led_array": [ { "id": 0, "r": 255, "g": 255, "b": 0 }, { "id": 5, "r": 128, "g": 0,   "b": 128 } ] } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "off" } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "single", "ledIndex": 12, "r": 255, "g": 255, "b": 255 } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "left", "r": 255, "g": 255, "b": 255 } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "rings", "radius": 4, "r": 255, "g": 255, "b": 255 } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "circles", "radius": 4, "r": 255, "g": 255, "b": 255 } }

	
		// 1) Check for "task"
		cJSON *task = cJSON_GetObjectItem(root, "task");
		if (!task || strcmp(task->valuestring, "/ledarr_act") != 0)
		{
			log_e("parseLedCommand: Invalid task: %s", task ? task->valuestring : "NULL");
			// Not our command structure
			return false;
		}
	
		// 2) Extract qid (if present)
		cmd.qid = 0;
		cJSON *jqid = cJSON_GetObjectItem(root, "qid");
		if (jqid && cJSON_IsNumber(jqid))
		{
			cmd.qid = jqid->valueint;
		}
	
		// 3) Look for the "led" object
		cJSON *ledObj = cJSON_GetObjectItem(root, "led");
		if (!ledObj || !cJSON_IsObject(ledObj))
		{
			log_e( "parseLedCommand: No 'led' object found");
			// If there's no "led" object, we can’t parse further
			return false;
		}
	
		// 4) Read LEDArrMode from "led" if needed
		//    (Might map to an internal mode or just store it.)
		cJSON *jMode = cJSON_GetObjectItem(ledObj, "LEDArrMode");
		if (jMode && cJSON_IsNumber(jMode))
		{
			int modeVal = jMode->valueint;
			log_i("parseLedCommand: LEDArrMode: %d", modeVal);
			// If you want to store it somewhere:
			// cmd.ledArrMode = modeVal;
		}
	
		// 5) Decide the LedMode from "action"
		cmd.mode = LedMode::UNKNOWN;
		cJSON *jaction = cJSON_GetObjectItem(ledObj, "action");
		if (jaction && jaction->valuestring)
		{
			log_i( "ParseLedCommand: action: %s", jaction->valuestring);
			String actStr = jaction->valuestring;
			actStr.toLowerCase();
			if (actStr == "rings")         { cmd.mode = LedMode::RINGS; }
			else if (actStr == "circles")  { cmd.mode = LedMode::CIRCLE; }
			else if (actStr == "halves")   { cmd.mode = LedMode::HALVES; }
			else if (actStr == "fill")     { cmd.mode = LedMode::FILL; }
			else if (actStr == "single")   { cmd.mode = LedMode::SINGLE; }
			else if (actStr == "off")      { cmd.mode = LedMode::OFF; }
			else if (actStr == "array")    { cmd.mode = LedMode::ARRAY; }
			// else remains UNKNOWN
		}
	
		// 6) Parse color (r, g, b)
		cmd.r = 0;
		cmd.g = 0;
		cmd.b = 0;
		cJSON *jr = cJSON_GetObjectItem(ledObj, "r");
		cJSON *jg = cJSON_GetObjectItem(ledObj, "g");
		cJSON *jb = cJSON_GetObjectItem(ledObj, "b");
		if (jr && cJSON_IsNumber(jr)) { cmd.r = jr->valueint; }
		if (jg && cJSON_IsNumber(jg)) { cmd.g = jg->valueint; }
		if (jb && cJSON_IsNumber(jb)) { cmd.b = jb->valueint; }
	
		// 7) Parse radius (for circles/rings)
		cmd.radius = 0;
		cJSON *jradius = cJSON_GetObjectItem(ledObj, "radius");
		if (jradius && cJSON_IsNumber(jradius))
		{
			log_i( "parseLedCommand: radius: %d", jradius->valueint);
			cmd.radius = jradius->valueint;
		}
	
		// 8) Parse region ("left", "right", "top", "bottom")
		memset(cmd.region, 0, sizeof(cmd.region));
		cJSON *jregion = cJSON_GetObjectItem(ledObj, "region");
		if (jregion && jregion->valuestring)
		{
			strncpy(cmd.region, jregion->valuestring, sizeof(cmd.region) - 1);
		}
	
		// 9) Parse single LED index
		cmd.ledIndex = 0;
		cJSON *jledIndex = cJSON_GetObjectItem(ledObj, "ledIndex");
		if (jledIndex && cJSON_IsNumber(jledIndex))
		{
			log_i( "parseLedCommand: ledIndex: %d", jledIndex->valueint);
			cmd.ledIndex = jledIndex->valueint;
		}
	
		// 10) If mode == ARRAY, parse "led_array"
		//     (not fully implemented here, just an example)
		cJSON *ledarr = cJSON_GetObjectItem(ledObj, "led_array");
		if (ledarr && cJSON_IsArray(ledarr))
		{
			// You can iterate over each item in the array:
			// cJSON *item = NULL;
			// cJSON_ArrayForEach(item, ledarr)
			// {
			//   int id   = cJSON_GetObjectItem(item, "id")->valueint;
			//   int rr   = cJSON_GetObjectItem(item, "r")->valueint;
			//   int gg   = cJSON_GetObjectItem(item, "g")->valueint;
			//   int bb   = cJSON_GetObjectItem(item, "b")->valueint;
			//   ... 
			// }
		}
	
		return true; // Successfully parsed
	}
	
	// ------------------------------------------------
	// 12) Execute a LedCommand
	// ------------------------------------------------
	void execLedCommand(const LedCommand &cmd)
	{
		switch (cmd.mode)
		{
		case LedMode::OFF:
			turnOff();
			break;
		case LedMode::FILL:
			fillAll(cmd.r, cmd.g, cmd.b);
			break;
		case LedMode::SINGLE:
			setSingle(cmd.ledIndex, cmd.r, cmd.g, cmd.b);
			break;
		case LedMode::HALVES:
			fillHalves(cmd.region, cmd.r, cmd.g, cmd.b);
			break;
		case LedMode::RINGS:
			drawRings(cmd.radius, cmd.r, cmd.g, cmd.b);
			break;
		case LedMode::CIRCLE:
			drawCircle(cmd.radius, cmd.r, cmd.g, cmd.b);
			break;
		case LedMode::ARRAY:
			// Here you could parse an array of LED changes.
			// For brevity, not implemented in detail.
			// e.g. cJSON_GetObjectItem(root,"led_array")...
			// Then set each LED individually
			// matrix->show();
			break;
		default:
			// Unknown => do nothing or turn off
			break;
		}
	}

	// ------------------------------------------------
	// 13) The main 'act' function that the user calls
	//     to pass cJSON with LED commands
	// ------------------------------------------------
	int act(cJSON *root)
	{
		LedCommand cmd;
		if (!parseLedCommand(root, cmd))
		{
			// Invalid or missing "task": "/led_arr"
			return -1;
		}
		#ifdef CAN_CONTROLLER and defined(CAN_MASTER)
		// Send the command to the CAN driver
		can_controller::sendLedCommandToCANDriver(cmd, pinConfig.CAN_ID_LED_0);
		#else
		execLedCommand(cmd);
		#endif
		return cmd.qid; // return the same QID
	}

	// ------------------------------------------------
	// 14) Optional 'get' function
	//     e.g. returns current state in JSON
	// ------------------------------------------------
	cJSON *get(cJSON *root)
	{
		// Example:
		// {
		//   "led": {
		//      "isOn": true,
		//      "count": LED_COUNT
		//   }
		// }
		cJSON *j = cJSON_CreateObject();
		cJSON *ld = cJSON_CreateObject();
		cJSON_AddItemToObject(j, "led", ld);

		cJSON_AddBoolToObject(ld, "isOn", isOn);
		cJSON_AddNumberToObject(ld, "count", LED_COUNT);

		// Could add more info about the last command etc.
		return j;
	}

	void cross_changed_event(int pressed)
	{
		if (pressed && isOn)
			return;
		log_i("Turn on LED ");
		LedController::fillAll(pinConfig.JOYSTICK_MAX_ILLU, pinConfig.JOYSTICK_MAX_ILLU, pinConfig.JOYSTICK_MAX_ILLU);
		isOn = true;
	}

	void circle_changed_event(int pressed)
	{
		if (pressed && !isOn)
			return;
		log_i("Turn off LED ");
		LedController::fillAll(0, 0, 0);
		isOn = false;
	}

	void loop()
	{

	}
};
