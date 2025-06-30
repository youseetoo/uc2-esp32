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

		if (x < 0 || x >= pinConfig.MATRIX_W || y < 0 || y >= pinConfig.MATRIX_H)
			return;
#ifdef HUB75
		matrix->drawPixel(x, y, rgb565(r, g, b));
#else
		uint16_t idx = xyToIndex(x, y);
		matrix->setPixelColor(idx, matrix->Color(r, g, b));
#endif
	}

	// ------------------------------------------------
	// 4) Setup: Initialize the matrix
	// ------------------------------------------------
	void setup()
	{

#ifdef DOTSTAR
		matrix = new Adafruit_DotStar(LED_COUNT, pinConfig.LED_PIN, pinConfig.LED_CLK, DOTSTAR_BGR);
#elif defined(HUB75)
		matrix = new Adafruit_Protomatter(pinConfig.MATRIX_W,
										  pinConfig.HUB75_BIT_DEPTH,
										  1, // one chain
										  const_cast<uint8_t *>(pinConfig.HUB75_RGB_PINS),
										  5,
										  const_cast<uint8_t *>(pinConfig.HUB75_ADDR_PINS),
										  pinConfig.HUB75_CLK,
										  pinConfig.HUB75_LAT,
										  pinConfig.HUB75_OE,
										  true); // double-buffer

		if (matrix->begin() != 0)
		{
			Serial.println("Protomatter init failed");
			while (1)
				delay(10);
		}

#else
		matrix = new Adafruit_NeoPixel(LED_COUNT, pinConfig.LED_PIN, NEO_GRB + NEO_KHZ800); // NEO_KHZ800);
		matrix->begin();
		matrix->setBrightness(255); // moderate brightness
		matrix->clear();
		matrix->show();
#endif

		// test led array
		int initIntensity = 100;
		fillAll(initIntensity, initIntensity, initIntensity);
		delay(10);
		fillAll(0, 0, 0);
		matrix->show(); //  Update strip to match

		isOn = false;

#ifdef WAVESHARE_ESP32S3_LEDARRAY
		// This will enable the masterboard to take over control of the LED array, too (alongside the firmware)
		Serial.println("WAVESHARE_ESP32S3_LEDARRAY");
		pinMode(14, OUTPUT);
		pinMode(6, INPUT);
		gpio_matrix_in(6, SIG_IN_FUNC212_IDX, false);
		gpio_matrix_out(14, SIG_IN_FUNC212_IDX, false, false);
#endif
	}

	// ------------------------------------------------
	// 5) "Off" all LEDs
	// ------------------------------------------------
	void turnOff()
	{
#ifdef HUB75
		matrix->fillScreen(0);
		matrix->show();
#else
		matrix->clear();
		matrix->show();
#endif
		isOn = false;
	}

	// ------------------------------------------------
	// 6) Fill entire matrix with a single color
	// ------------------------------------------------
	void fillAll(uint8_t r, uint8_t g, uint8_t b)
	{
#ifdef HUB75
		uint16_t c = rgb565(r, g, b);
		matrix->fillScreen(c);
#else
		for (uint16_t i = 0; i < LED_COUNT; i++)
			matrix->setPixelColor(i, matrix->Color(r, g, b));
#endif
		matrix->show();
		isOn = (r || g || b);
	}

	// ------------------------------------------------
	// 7) Light a SINGLE LED by index
	// ------------------------------------------------
	void setSingle(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
	{
#ifdef HUB75
		matrix->fillScreen(0);
		int x = index % pinConfig.MATRIX_W;
		int y = index / pinConfig.MATRIX_W;
		matrix->drawPixel(x, y, rgb565(r, g, b));
		matrix->show();
#else
		if (index < LED_COUNT)
		{
			matrix->clear();
			matrix->setPixelColor(index, matrix->Color(r, g, b));
			matrix->show();
		}
#endif
		isOn = (r || g || b);
	}

	// ------------------------------------------------
	// 8) Halves: fill left/right/top/bottom with color, rest dark
	// ------------------------------------------------
	void fillHalves(const char *region, uint8_t r, uint8_t g, uint8_t b)
	{
#ifndef HUB75

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
#endif
	}

	// ------------------------------------------------
	// 9) RINGS: for demonstration, fill a big circle,
	//           then carve out the interior to simulate a ring
	//           or you can do multiple concentric rings, etc.
	//           For illumination board with discrete rings, use direct indexing
	// ------------------------------------------------
	void drawRings(uint8_t radius, uint8_t r, uint8_t g, uint8_t b)
	{
#ifndef HUB75
		// Check if this is an illumination board with defined ring structure
		#ifdef LED_CONTROLLER
		// Check if we have ring definitions in the pin config (illumination board)
		if (pinConfig.pindefName && 
			strcmp(pinConfig.pindefName, "seeed_xiao_esp32s3_can_slave_illumination") == 0)
		{
			// Use direct ring indexing for illumination board
			drawIlluminationRings(radius, r, g, b);
			return;
		}
		#endif

		// Default matrix-based ring drawing for other configurations
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
#endif
	}

	// ------------------------------------------------
	// 9b) ILLUMINATION RINGS: Handle discrete ring addressing
	//     for the UC2 illumination board with 4 concentric rings
	// ------------------------------------------------
	void drawIlluminationRings(uint8_t ring_id, uint8_t r, uint8_t g, uint8_t b)
	{
		// Hard-coded ring mapping for illumination board
		// Ring mapping: radius 0=inner, 1=middle, 2=biggest, 3=outest
		uint16_t start_idx = 0;
		uint16_t count = 0;

		switch (ring_id) {
			case 0: // Inner ring
				start_idx = pinConfig.RING_INNER_START;   
				count = pinConfig.RING_INNER_COUNT;      
				break;
			case 1: // Middle ring
				start_idx = pinConfig.RING_MIDDLE_START;  
				count = pinConfig.RING_MIDDLE_COUNT;      
				break;
			case 2: // Biggest ring
				start_idx = pinConfig.RING_BIGGEST_START;  
				count = pinConfig.RING_BIGGEST_COUNT;      
				break;
			case 3: // Outest ring
				start_idx = pinConfig.RING_OUTEST_START;  
				count = pinConfig.RING_OUTEST_COUNT;      
				break;
			default:
				// Invalid ring, light up all rings
				uint16_t total_leds = pinConfig.RING_OUTEST_START + pinConfig.RING_OUTEST_COUNT;
				for (uint16_t i = 0; i < total_leds; i++) {
					matrix->setPixelColor(i, matrix->Color(r, g, b));
				}
				matrix->show();
				isOn = (r || g || b);
				return;
		}

		// Clear all LEDs first
		matrix->clear();
		
		// Set the specified ring
		for (uint16_t i = 0; i < count; i++) {
			matrix->setPixelColor(start_idx + i, matrix->Color(r, g, b));
		}
		
		matrix->show();
		isOn = (r || g || b);
	}

	// ------------------------------------------------
	// 9c) ILLUMINATION RING SEGMENTS: Handle ring segments (left/right/top/bottom)
	//     for the UC2 illumination board
	// ------------------------------------------------
	void drawIlluminationRingSegment(uint8_t ring_id, const char* region, uint8_t r, uint8_t g, uint8_t b)
	{
		// Get ring parameters
		uint16_t start_idx = 0;
		uint16_t count = 0;
		log_i("drawIlluminationRingSegment: ring_id=%d, region=%s, r=%d, g=%d, b=%d", ring_id, region, r, g, b);
		switch (ring_id) {
			case 0: start_idx = pinConfig.RING_INNER_START; count = pinConfig.RING_INNER_COUNT; break;   // Inner ring
			case 1: start_idx = pinConfig.RING_MIDDLE_START; count = pinConfig.RING_MIDDLE_COUNT; break;  // Middle ring  
			case 2: start_idx = pinConfig.RING_BIGGEST_START; count = pinConfig.RING_BIGGEST_COUNT; break;  // Biggest ring
			case 3: start_idx = pinConfig.RING_OUTEST_START; count = pinConfig.RING_OUTEST_COUNT; break;  // Outest ring
			default: return; // Invalid ring
		}

		// Clear all LEDs first
		matrix->clear();

		// Calculate segment within the ring (relative to ring start)
		uint16_t relative_start = 0;
		uint16_t segment_count = count / 2; // Half ring for each segment

		if (strcasecmp(region, "top") == 0) {
			relative_start = 0;                 // Start at beginning of ring
		}
		else if (strcasecmp(region, "right") == 0) {
			relative_start = count / 4;         // 25% into the ring
		}
		else if (strcasecmp(region, "bottom") == 0) {
			relative_start = count / 2;         // 50% into the ring
		}
		else if (strcasecmp(region, "left") == 0) {
			relative_start = (3 * count) / 4;   // 75% into the ring
		}
		else {
			// Unknown region, light up whole ring
			relative_start = 0;
			segment_count = count;
		}

		// Set the specified ring segment with proper wraparound
		for (uint16_t i = 0; i < segment_count; i++) {
			uint16_t led_idx = start_idx + ((relative_start + i) % count);
			matrix->setPixelColor(led_idx, matrix->Color(r, g, b));
		}
		
		matrix->show();
		isOn = (r || g || b);
	}

	// ------------------------------------------------
	// 10) Filled circle
	// ------------------------------------------------
	void drawCircle(uint8_t radius, uint8_t rVal, uint8_t gVal, uint8_t bVal)
	{
#ifndef HUB75
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
#endif
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
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "fill", "r": 255, "g": 255, "b": 255 } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "single", "ledIndex": 12, "r": 255, "g": 255, "b": 255 } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "left", "r": 255, "g": 255, "b": 255 } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "right", "r": 15, "g": 15, "b": 15} }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "rings", "radius": 1, "r": 255, "g": 255, "b": 255 } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "circles", "radius": 2, "r": 255, "g": 255, "b": 255 } }
		// { "task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"idle" } }
		// { "task": "/ledarr_act", "qid": 17, "led"}

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
			log_e("parseLedCommand: No 'led' object found");
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
			log_i("ParseLedCommand: action: %s", jaction->valuestring);
			String actStr = jaction->valuestring;
			actStr.toLowerCase();
			if (actStr == "rings")
			{
				cmd.mode = LedMode::RINGS;
			}
			else if (actStr == "circles")
			{
				cmd.mode = LedMode::CIRCLE;
			}
			else if (actStr == "halves")
			{
				cmd.mode = LedMode::HALVES;
			}
			else if (actStr == "fill")
			{
				cmd.mode = LedMode::FILL;
			}
			else if (actStr == "single")
			{
				cmd.mode = LedMode::SINGLE;
			}
			else if (actStr == "off")
			{
				cmd.mode = LedMode::OFF;
			}
			else if (actStr == "array")
			{
				cmd.mode = LedMode::ARRAY;
			}
			else if (actStr == "status")
			{
				cmd.mode = LedMode::STATUS;
			}
			else if (actStr == "unknown")
			{
				cmd.mode = LedMode::UNKNOWN;
			}
			else
			{
				log_e("parseLedCommand: Unknown action: %s", actStr.c_str());
				return false; // Invalid action
			}
			// else remains UNKNOWN
		}

		// 6) Parse color (r, g, b)
		cmd.r = 0;
		cmd.g = 0;
		cmd.b = 0;
		cJSON *jr = cJSON_GetObjectItem(ledObj, "r");
		cJSON *jg = cJSON_GetObjectItem(ledObj, "g");
		cJSON *jb = cJSON_GetObjectItem(ledObj, "b");
		if (jr && cJSON_IsNumber(jr))
		{
			cmd.r = jr->valueint;
		}
		if (jg && cJSON_IsNumber(jg))
		{
			cmd.g = jg->valueint;
		}
		if (jb && cJSON_IsNumber(jb))
		{
			cmd.b = jb->valueint;
		}

		// 7) Parse radius (for circles/rings)
		cmd.radius = 0;
		cJSON *jradius = cJSON_GetObjectItem(ledObj, "radius");
		if (jradius && cJSON_IsNumber(jradius))
		{
			log_i("parseLedCommand: radius: %d", jradius->valueint);
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
			log_i("parseLedCommand: ledIndex: %d", jledIndex->valueint);
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

		// 11) If mode == STATUS, you can add more status info here
		//     (e.g. we want to address e.g. red, green, blue, rainbow for status on the FRAME )
		if (cmd.mode == LedMode::STATUS)
		{
			// {"task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"idle" } }
			// {"task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"error" } }
			// {"task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"rainbow" } }
			// we parse the incoming status to LedForStatus
			// and then we can set the color of the LED in the loop to e.g. rainbow, red/green/blue glowing
			cJSON *jstatus = cJSON_GetObjectItem(ledObj, "status");
			if (jstatus && jstatus->valuestring)
			{
				String statusStr = jstatus->valuestring;
				statusStr.toLowerCase();
				if (statusStr == "warn")
				{
					currentLedForStatus = LedForStatus::warn;
				}
				else if (statusStr == "error")
				{
					currentLedForStatus = LedForStatus::error;
				}
				else if (statusStr == "idle")
				{
					currentLedForStatus = LedForStatus::idle;
				}
				else if (statusStr == "success")
				{
					currentLedForStatus = LedForStatus::success;
				}
				else if (statusStr == "busy")
				{
					currentLedForStatus = LedForStatus::busy;
				}
				else if (statusStr == "rainbow")
				{
					currentLedForStatus = LedForStatus::rainbow;
				}
				else
				{
					log_e("parseLedCommand: Unknown status: %s", statusStr.c_str());
					currentLedForStatus = LedForStatus::unknown;
					return false; // Invalid status
				}
			}
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
			// For illumination board, check if region is specified for ring segments
			if (pinConfig.pindefName && 
				strcmp(pinConfig.pindefName, "seeed_xiao_esp32s3_can_slave_illumination") == 0 &&
				strlen(cmd.region) > 0)
			{
				drawIlluminationRingSegment(cmd.radius, cmd.region, cmd.r, cmd.g, cmd.b);
			}
			else
			{
				drawRings(cmd.radius, cmd.r, cmd.g, cmd.b);
			}
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
#if defined(CAN_CONTROLLER) && defined(CAN_MASTER) && !defined(CAN_SLAVE_LED)
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
		// {"task": "/ledarr_get", "qid": 17, "led": { "isOn": true, "count": 64 }}
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
		{
			Serial.println("Cross pressed, but LED is on");
			return;
		}
		else
		{
			Serial.println("Cross pressed, LED is off");
			// differentiate between CAN MASTER and CAN SLAVE
			LedCommand cmd;
			cmd.mode = LedMode::CIRCLE;
			cmd.r = pinConfig.JOYSTICK_MAX_ILLU;
			cmd.g = pinConfig.JOYSTICK_MAX_ILLU;
			cmd.b = pinConfig.JOYSTICK_MAX_ILLU;
			cmd.radius = 8;	   // radius of the circle
			cmd.ledIndex = 0;  // not used
			cmd.region[0] = 0; // not used
			cmd.qid = 0;	   // not used
			isOn = true;
#if defined(CAN_CONTROLLER) && defined(CAN_MASTER) && !defined(CAN_SLAVE_LED)
			// Send the command to the CAN driver
			can_controller::sendLedCommandToCANDriver(cmd, pinConfig.CAN_ID_LED_0);
#else
			// Execute the command directly
			execLedCommand(cmd);
#endif
		}

	}

	void circle_changed_event(int pressed)
	{
		if (pressed && !isOn)
		{
			Serial.println("Circle pressed, but LED is off");
			return;
		}
		else
		{
			Serial.println("Circle pressed, LED is on");
			// differentiate between CAN MASTER and CAN SLAVE
			LedCommand cmd;
			cmd.mode = LedMode::CIRCLE;
			cmd.r = 0;		   // pinConfig.JOYSTICK_MAX_ILLU;
			cmd.g = 0;		   // pinConfig.JOYSTICK_MAX_ILLU;
			cmd.b = 0;		   // pinConfig.JOYSTICK_MAX_ILLU;
			cmd.radius = 8;	   // radius of the circle
			cmd.ledIndex = 0;  // not used
			cmd.region[0] = 0; // not used
			cmd.qid = 0;	   // not used
			isOn = false;
#if defined(CAN_CONTROLLER) && defined(CAN_MASTER) && !defined(CAN_SLAVE_LED)
			// Send the command to the CAN driver
			can_controller::sendLedCommandToCANDriver(cmd, pinConfig.CAN_ID_LED_0);
#else
			// Execute the command directly
			execLedCommand(cmd);
#endif
		}
	}

	void loop()
	{

#ifndef HUB75
// only on the HAT MAster => Glow or show status
if( pinConfig.pindefName && 
	strcmp(pinConfig.pindefName, "UC2_3_CAN_HAT_Master") == 0 )	
{
		// Determine color based on currentLedForStatus
		uint32_t color = 0;
		// Fade the brightnessLoop up/down
		brightnessLoop += (fadeDirection * 5);
		if (brightnessLoop == 0 || brightnessLoop == 255)
		{
			fadeDirection = -fadeDirection;
		}
		switch (currentLedForStatus)
		{
		case LedForStatus::busy:
			// Busy => Yellow
			color = matrix->Color(brightnessLoop, brightnessLoop, 0);
			break;
		case LedForStatus::error:
			// Error => Red
			color = matrix->Color(brightnessLoop, 0, 0);
			break;
		case LedForStatus::idle:
			// Idle => Green
			color = matrix->Color(0, brightnessLoop, 0);
			break;
		case LedForStatus::rainbow:
		{
			// Rainbow => show a color gradient across the strip
			for (uint16_t i = 0; i < LED_COUNT; i++)
			{
				// Create a hue offset for each pixel, adding brightnessLoop helps shift the pattern
				uint8_t hue = (brightnessLoop + i * 256 / LED_COUNT) & 0xFF;
				// Convert HSV to RGB, apply gamma correction
				uint32_t c = matrix->gamma32(matrix->ColorHSV((uint16_t)hue << 8, 255, 255));
				matrix->setPixelColor(i, c);
			}
			matrix->show();
			return; // skip the rest
		}
		case LedForStatus::on_:
			// On => White
			color = matrix->Color(255, 255, 255);
			break;
		case LedForStatus::off_:
			// Off => Black
			color = matrix->Color(0, 0, 0);
			break;
		default:
			// Unknown => do nothing
			return;
		}


	}
#endif // HUB75
}

#ifdef HUB75
	uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
	{
		return matrix->color565(r, g, b);
	}
#endif

};
