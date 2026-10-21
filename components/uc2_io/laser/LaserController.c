// T4.1: pure C (was the C++ namespace LaserController).
// Serial output goes through serial_out_safe_send_json_string; laser act
// documents for dpad/cross events go through the C shim
// device_router_handle_laser_act (uc2_canopen).
#include <PinConfig.h>
#include "LaserController.h"
// T1.3: direct IDF (no Arduino) — uc2 façades for ledc/gpio/time/misc/log.
// (Sub-headers, not the C++-only uc2_hal.h umbrella.)
// JsonKeys.h dropped (Arduino/pgmspace): key_laser == "laser".
#include "uc2_hal/uc2_ledc.h"   // ledcSetup / ledcAttachPin / ledcDetachPin / ledcWrite
#include "uc2_hal/uc2_misc.h"   // random / map
#include "uc2_hal/uc2_gpio.h"   // pinMode / digitalWrite / OUTPUT / LOW
#include "uc2_hal/uc2_time.h"   // millis / delay
#include "uc2_hal/uc2_log.h"    // log_i / log_w / log_e / log_d
#include <stdlib.h>
#include "cJsonTool.h"
#include "State.h"
#include "uc2_serial_out.h"  // serial_out_safe_send_json_string (C API)
#include "QidRegistry.h"
#include "DeviceRouter.h"
#ifdef I2C_LASER
// C shim over the i2c_master namespace (uc2_periph); implemented as an
// extern "C" wrapper when the periph component is C-ported.
void i2c_master_send_laser_data(const LaserData *laser_data, uint8_t id);
#endif

static const char *TAG = "LaserController";

// ── State (was namespace statics) ─────────────────────────────────────
int laser_val_arr[LASER_MAX_LASERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int laser_despeckle_arr[LASER_MAX_LASERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int laser_despeckle_period_arr[LASER_MAX_LASERS] = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20};

// PWM channel mapping (0=Heating, 1-4=Lasers).
int laser_pwm_channel[LASER_MAX_LASERS] = {4, 1, 2, 3, 5, 0, 0, 0, 0, 0};

// PWM configuration.
static int pwm_resolution = 10; // 8bit 256, 10bit 1024, 12bit 4096
static int pwm_frequency = 5000; // 19000; 12000
static long pwm_max = 1L << 10;

// Flags to track pending laser value updates that need to be sent.
static bool laserValuePending[LASER_MAX_LASERS] = {false};
static int pendingQid[LASER_MAX_LASERS] = {0};

static int minPulseWidth = 500;
static int maxPulseWidth = 2500;

static bool laser_on = false;
static bool laser2_on = false;

// Variables for hold/release detection (0=UP, 1=DOWN, 2=RIGHT, 3=LEFT).
static unsigned long pressStartTime[4] = {0, 0, 0, 0};
static bool isPressed[4] = {false, false, false, false};
static bool isHolding[4] = {false, false, false, false};
static unsigned long lastHoldAction[4] = {0, 0, 0, 0};
static const unsigned long HOLD_THRESHOLD = 500; // ms to detect hold
static const unsigned long HOLD_INCREMENT_INTERVAL = 200; // ms between increments

// Cross button toggle for Laser 4.
static unsigned long crossLastEventTime = 0;
static const unsigned long BUTTON_DEBOUNCE_TIME = 300; // ms to prevent multiple toggles
static bool laser4ToggleState = false; // false = off/min, true = on/max

// Helper function to get laser pin by ID
int laser_get_pin(int LASERid)
{
	switch (LASERid)
	{
	case 0:
		return pinConfig.LASER_0;
	case 1:
		return pinConfig.LASER_1;
	case 2:
		return pinConfig.LASER_2;
	case 3:
		return pinConfig.LASER_3;
	case 4:
		return pinConfig.LASER_4;
	default:
		return -1;
	}
}

// Helper function to get PWM channel by ID
int laser_get_pwm_channel(int LASERid)
{
	if (LASERid >= 0 && LASERid < LASER_MAX_LASERS)
	{
		return laser_pwm_channel[LASERid];
	}
	return -1;
}

void laser_despeckle(int LASERdespeckle, int LASERid, int LASERperiod)
{
	if (LASERid < 0 || LASERid >= LASER_MAX_LASERS)
	{
		log_w("Invalid LASERid %i for despeckle", LASERid);
		return;
	}

	log_e("LASERdespeckle %i, LASERid %i, LASERperiod %i", LASERdespeckle, LASERid, LASERperiod);

	int LASER_val_wiggle = laser_val_arr[LASERid];
	int PWM_CHANNEL = laser_get_pwm_channel(LASERid);

	// Add random number to current value to let it oscillate
	int32_t laserwiggle = (int32_t)random(-LASERdespeckle, LASERdespeckle);
	LASER_val_wiggle += laserwiggle;

	if (LASER_val_wiggle > pwm_max)
		LASER_val_wiggle -= (2 * abs(laserwiggle));
	if (LASER_val_wiggle < 0)
		LASER_val_wiggle += (2 * abs(laserwiggle));

	log_d("%i", LASERid);
	log_d("%i", LASER_val_wiggle);

	laser_set_pwm(LASER_val_wiggle, PWM_CHANNEL);
	delay(LASERperiod);
}

void laser_set_pwm(int pwmValue, int pwmChannel)
{
	// sets the PWM value for the given channel
	log_i("Setting PWM value %i on channel %i", pwmValue, pwmChannel);
	ledcWrite(pwmChannel, pwmValue);
}

// Custom function accessible by the API
int laser_act(cJSON *ob)
{
	// JSON String
	// {"task":"/laser_act", "LASERid":1, "LASERval":1000}, "LASERdespeckle":0, "LASERdespecklePeriod":0}
	// {"task":"/laser_act", "LASERid":2, "LASERval":1000}
	// {"task":"/laser_act", "LASERid":2, "LASERval":10000}
	state_set_busy(true);
	int qid = cjson_tool_get_int(ob, "qid", 0);
	cJSON *setPWMFreq = cJSON_GetObjectItemCaseSensitive(ob, "LASERFreq");
	cJSON *setPWMRes = cJSON_GetObjectItemCaseSensitive(ob, "LASERRes");
	bool hasLASERval = cJSON_HasObjectItem(ob, "LASERval"); // check if ob contains the key "LASERval"
	bool isServo = cJSON_HasObjectItem(ob, "servo"); // check if ob contains the key "servo"

	// assign values
	int LASERid = 0;
	int LASERval = 0;
	int LASERdespeckle = 0;
	int LASERdespecklePeriod = 0;
	// default values overridden
	LASERid = cjson_tool_get_int(ob, "LASERid", 0);
	LASERval = cjson_tool_get_int(ob, "LASERval", 0);
	LASERdespeckle = cjson_tool_get_int(ob, "LASERdespeckle", 0);
	LASERdespecklePeriod = cjson_tool_get_int(ob, "LASERdespecklePeriod", 0);

	// debugging
	log_i("LaserID %i, LaserVal %i, LaserDespeckle %i, LaserDespecklePeriod %i", LASERid, LASERval, LASERdespeckle, LASERdespecklePeriod);

	// Validate LASERid
	if (LASERid < 0 || LASERid >= LASER_MAX_LASERS)
	{
		log_w("Invalid LASERid: %d", LASERid);
		state_set_busy(false);
		return 0;
	}

	int laserPin = laser_get_pin(LASERid);
	int pwmChannel = laser_get_pwm_channel(LASERid);

// Check if laser pin is configured for native laser control
// Skip this check for I2C laser boards
#if !defined(I2C_LASER)
	if (laserPin < 0)
	{
		log_w("Laser pin not configured for LASERid %d", LASERid);
		state_set_busy(false);
		return 0;
	}
#endif

	/*
	Set Laser PWM Frequency
	*/
	if (setPWMFreq != NULL && !isServo)
	{
		// {"task":"/laser_act", "LASERid":1 ,"LASERFreq":50, "LASERval":1000, "qid":1}
		pwm_frequency = setPWMFreq->valueint;
		log_i("Setting PWM frequency to %i for LASERid %i", pwm_frequency, LASERid);
		laser_setup_channel(laserPin, pwmChannel, pwm_frequency, pwm_resolution);
	}

	/*
	Set Laser PWM Resolution
	*/
	if (setPWMRes != NULL && !isServo)
	{
		// {"task":"/laser_act", "LASERid":2 ,"LASERRes":16}
		pwm_resolution = setPWMRes->valueint;
		log_i("Setting PWM resolution to %i for LASERid %i", pwm_resolution, LASERid);
		laser_setup_channel(laserPin, pwmChannel, pwm_frequency, pwm_resolution);
	}

	// Handle laser value setting
	if (hasLASERval)
	{
		if (isServo)
		{
			// Servo mode
			// {"task":"/laser_act", "LASERid":1 ,"LASERval":99, "servo":1, "qid":1}
			pwm_frequency = 50;
			pwm_resolution = 16;

			laser_configure_pwm(laserPin, pwm_resolution, pwmChannel, pwm_frequency);
			laser_move_servo(pwmChannel, LASERval, pwm_frequency, pwm_resolution);
		}
		else
		{
			// Normal laser mode - use setLaserVal_full with despeckle parameters
			laser_set_val_full(LASERid, LASERval, LASERdespeckle, LASERdespecklePeriod, qid);
		}

		log_i("LASERid %i, LASERval %i", LASERid, LASERval);
		// Laser is synchronous: register and immediately report done
		if (qid > 0)
		{
			qid_register(qid, 1);
			qid_report_action_done(qid);
		}
		state_set_busy(false);
		return qid;
	}

	// No LASERval provided - still report QID done if set
	if (qid > 0)
	{
		qid_register(qid, 1);
		qid_report_action_done(qid);
	}
	return qid;
}

void laser_apply_value(const LaserData *laser_data)
{
#ifdef I2C_LASER
	i2c_master_send_laser_data(laser_data, (uint8_t)laser_data->LASERid);
#else
	int LASERid = laser_data->LASERid;

	// Validate LASERid
	if (LASERid < 0 || LASERid >= LASER_MAX_LASERS)
	{
		log_w("Invalid LASERid: %d", LASERid);
		return;
	}

	// Set value in array
	laser_val_arr[LASERid] = laser_data->LASERval;

	// Get PWM channel and apply
	int pwmChannel = laser_get_pwm_channel(LASERid);
	if (pwmChannel != -1)
	{
		laser_set_pwm(laser_data->LASERval, pwmChannel);
	}
#endif
}

bool laser_set_val(int LASERid, int LASERval)
{
	// Use current despeckle values from arrays, no qid.
	return laser_set_val_full(LASERid, LASERval, laser_despeckle_arr[LASERid], laser_despeckle_period_arr[LASERid], 0);
}

bool laser_set_val_full(int LASERid, int LASERval, int LASERdespeckle, int LASERdespecklePeriod, int qid)
{
	log_i("Setting Laser Value: LASERid %i, LASERval %i, despeckle %i, period %i, qid %i",
		  LASERid, LASERval, LASERdespeckle, LASERdespecklePeriod, qid);

	// Validate LASERid
	if (LASERid < 0 || LASERid >= LASER_MAX_LASERS)
	{
		log_w("Invalid LASERid: %d", LASERid);
		return false;
	}

	// Store qid for this laser
	pendingQid[LASERid] = qid;

	// Update arrays with new values
	laser_val_arr[LASERid] = LASERval;
	laser_despeckle_arr[LASERid] = LASERdespeckle;
	laser_despeckle_period_arr[LASERid] = LASERdespecklePeriod;

	// Apply PWM locally (CAN routing is handled by DeviceRouter)
	int laserPin = laser_get_pin(LASERid);
	if (laserPin <= 0)
	{
		log_w("Laser pin not configured for LASERid %d", LASERid);
		return false;
	}

	int pwmChannel = laser_get_pwm_channel(LASERid);
	laser_set_pwm(LASERval, pwmChannel);

	log_i("LASERid %i, LASERval %i", LASERid, LASERval);

	// Set flag to send update in next loop cycle
	laserValuePending[LASERid] = true;
	return true;
}

int laser_get_val(int LASERid)
{
	// Validate LASERid
	if (LASERid < 0 || LASERid >= LASER_MAX_LASERS)
	{
		log_w("Invalid LASERid: %d", LASERid);
		return 0;
	}

	int laserVal = laser_val_arr[LASERid];
	log_i("LASERid %i, LASERval %i", LASERid, laserVal);
	return laserVal;
}

void laser_send_value(int LASERid, int qid)
{
	// Send laser value update similar to sendMotorPos pattern
	// JSON format: {"laser": {"LASERid": X, "LASERval": Y}, "qid": Z}

	log_i("Sending laser value update: LASERid %i, LASERval %i, qid %i",
		  LASERid, laser_get_val(LASERid), qid);

	cJSON *root = cJSON_CreateObject();
	if (root == NULL)
	{
		log_e("Failed to create JSON object for laser update");
		return;
	}

	cJSON *laserObj = cJSON_CreateObject();
	if (laserObj == NULL)
	{
		cJSON_Delete(root);
		log_e("Failed to create laser object");
		return;
	}

	cJSON_AddItemToObject(root, "laser", laserObj);
	cJSON_AddNumberToObject(laserObj, "LASERid", LASERid);
	cJSON_AddNumberToObject(laserObj, "LASERval", laser_get_val(LASERid));
	cJSON_AddNumberToObject(laserObj, "isDone", true); // Laser is immediately done after setting

	if (qid != 0)
	{
		cJSON_AddNumberToObject(root, "qid", qid);
	}

	// Serialize to string BEFORE deleting the cJSON object
	char *jsonString = cJSON_PrintUnformatted(root);

	// Check if serialization was successful
	if (jsonString == NULL)
	{
		log_e("Failed to serialize laser value JSON for LASERid %d", LASERid);
		cJSON_Delete(root);
		return;
	}

	// Delete cJSON object immediately after serialization
	cJSON_Delete(root);
	root = NULL;

	// Send the pre-serialized string through the safe output queue
	serial_out_safe_send_json_string(jsonString);

	// Free the serialized string
	free(jsonString);
	jsonString = NULL;

	log_d("Laser value update sent successfully for LASERid %i", LASERid);
}

void laser_setup_channel(int laser_pin, int pwm_chan, int pwm_freq, int pwm_res)
{
	// Setting up the differen PWM channels for the laser
	log_i("Setting up Laser with PWM Channel %i, PWM Frequency %i, PWM Resolution %i", pwm_chan, pwm_freq, pwm_res);
	ledcSetup(pwm_chan, pwm_freq, pwm_res);
	ledcAttachPin(laser_pin, pwm_chan);
}

LaserData laser_get_data(void)
{
	LaserData laserData;
	laserData.LASERid = 1;
	laserData.LASERval = laser_val_arr[1];
	laserData.LASERdespeckle = laser_despeckle_arr[1];
	laserData.LASERdespecklePeriod = laser_despeckle_period_arr[1];
	return laserData;
}

void laser_dpad_changed_event(int pressed)
{
	/*
	Enhanced behavior:
	- Short click RIGHT => toggle laser 2 on/off (10000/0)
	- Long hold RIGHT => increment laser 2 in steps of 500
	- Short click LEFT => toggle laser 2 on/off (10000/0)
	- Long hold LEFT => decrement laser 2 in steps of 500
	- Short click UP => toggle laser 1 on/off (10000/0)
	- Long hold UP => increment laser 1 in steps of 500
	- Short click DOWN => toggle laser 1 on/off (10000/0)
	- Long hold DOWN => decrement laser 1 in steps of 500
	*/

	unsigned long currentTime = millis();

	if (pressed == LASER_DPAD_UP)
	{
		if (!isPressed[0]) // Button press started
		{
			isPressed[0] = true;
			pressStartTime[0] = currentTime;
			isHolding[0] = false;
		}
	}
	else if (pressed == LASER_DPAD_DOWN)
	{
		if (!isPressed[1]) // Button press started
		{
			isPressed[1] = true;
			pressStartTime[1] = currentTime;
			isHolding[1] = false;
		}
	}
	else if (pressed == LASER_DPAD_RIGHT)
	{
		if (!isPressed[2]) // Button press started
		{
			isPressed[2] = true;
			pressStartTime[2] = currentTime;
			isHolding[2] = false;
		}
	}
	else if (pressed == LASER_DPAD_LEFT)
	{
		if (!isPressed[3]) // Button press started
		{
			isPressed[3] = true;
			pressStartTime[3] = currentTime;
			isHolding[3] = false;
		}
	}
	else if (pressed == LASER_DPAD_NONE)
	{
		// Handle button releases
		for (int i = 0; i < 4; i++)
		{
			if (isPressed[i])
			{
				isPressed[i] = false;
				unsigned long pressDuration = currentTime - pressStartTime[i];

				if (!isHolding[i] && pressDuration < HOLD_THRESHOLD)
				{
					// Short click detected
					laser_handle_short_click(i);
				}
				isHolding[i] = false;
			}
		}
	}
}

void laser_handle_short_click(int direction)
{
	// 0=UP, 1=DOWN, 2=RIGHT, 3=LEFT
	int laserID = 0;
	int laserVal = 0;
	if (direction == 0 || direction == 1) // UP or DOWN - Laser 1
	{
		if (laser_on)
		{
			//TODO: needs to go through router
			log_i("Short click - Laser 1 OFF");
			laserID = 1;
			laserVal = 0;
			laser_on = false;
		}
		else
		{
			log_i("Short click - Laser 1 ON");
			laserID = 1;
			laserVal = 10000;
			laser_on = true;
		}
	}
	else if (direction == 2 || direction == 3) // RIGHT or LEFT - Laser 2
	{
		if (laser2_on)
		{
			log_i("Short click - Laser 2 OFF");
			laserID = 2;
			laserVal = 0;
			laser2_on = false;
		}
		else
		{
			log_i("Short click - Laser 2 ON");
			laserID = 2;
			laserVal = 10000;
			laser2_on = true;
		}
	}

	cJSON *doc = cJSON_CreateObject();
	cJSON_AddNumberToObject(doc, "LASERid", laserID);
	cJSON_AddNumberToObject(doc, "LASERval", laserVal);
	cJSON *resp = device_router_handle_laser_act(doc);
	if (resp)
		cJSON_Delete(resp);
	cJSON_Delete(doc);
}

// Call this from laser_loop() to handle hold actions
void laser_process_hold_actions(void)
{
	unsigned long currentTime = millis();

	for (int i = 0; i < 4; i++)
	{
		if (isPressed[i] && !isHolding[i] &&
			(currentTime - pressStartTime[i]) >= HOLD_THRESHOLD)
		{
			// Start hold action
			isHolding[i] = true;
			lastHoldAction[i] = currentTime;
			log_i("Hold started for direction %d", i);
		}

		if (isHolding[i] &&
			(currentTime - lastHoldAction[i]) >= HOLD_INCREMENT_INTERVAL)
		{
			// Execute hold action
			laser_execute_hold_action(i);
			lastHoldAction[i] = currentTime;
		}
	}
}

void laser_execute_hold_action(int direction)
{
	// 0=UP, 1=DOWN, 2=RIGHT, 3=LEFT
	int incrementValue = 100;
	int maxValue = 20000;
	if (direction == 0) // UP - increment Laser 1
	{
		int currentVal = laser_get_val(1);
		int newVal = currentVal + incrementValue;
		if (newVal > maxValue)
			newVal = maxValue;
		if (newVal != currentVal)
		{
			log_i("Hold UP - incrementing Laser 1: %d -> %d", currentVal, newVal);
			laser_set_val(1, newVal);
			laser_on = (newVal > 0);
		}
	}
	else if (direction == 1) // DOWN - decrement Laser 1
	{
		int currentVal = laser_get_val(1);
		int newVal = currentVal - incrementValue;
		if (newVal < 0)
			newVal = 0;
		if (newVal != currentVal)
		{
			log_i("Hold DOWN - decrementing Laser 1: %d -> %d", currentVal, newVal);
			laser_set_val(1, newVal);
			laser_on = (newVal > 0);
		}
	}
	else if (direction == 2) // RIGHT - increment Laser 2
	{
		int currentVal = laser_get_val(2);
		int newVal = currentVal + incrementValue;
		if (newVal > maxValue)
			newVal = maxValue;
		if (newVal != currentVal)
		{
			log_i("Hold RIGHT - incrementing Laser 2: %d -> %d", currentVal, newVal);
			laser_set_val(2, newVal);
			laser2_on = (newVal > 0);
		}
	}
	else if (direction == 3) // LEFT - decrement Laser 2
	{
		int currentVal = laser_get_val(2);
		int newVal = currentVal - incrementValue;
		if (newVal < 0)
			newVal = 0;
		if (newVal != currentVal)
		{
			log_i("Hold LEFT - decrementing Laser 2: %d -> %d", currentVal, newVal);
			laser_set_val(2, newVal);
			laser2_on = (newVal > 0);
		}
	}
}

void laser_move_servo(int ledChannel, int angle, int frequency, int resolution)
{
	// { "task": "/laser_act", "LASERid": 1, "LASERval": 170, "servo": 1, "qid": 1 }

	// Map the angle to the corresponding pulse width
	int pulseWidth = (int)map(angle, 0, 180, minPulseWidth, maxPulseWidth);
	// Convert the pulse width to a duty cycle value
	int dutyCycle = (int)map(pulseWidth, 0, 1000000 / frequency, 0, (1 << resolution) - 1);
	// Write the duty cycle to the LEDC channel
	ledcWrite(ledChannel, dutyCycle);
	delay(100); // Wait for the servo to reach the position
}

void laser_configure_pwm(int servoPin, int resolution, int ledChannel, int frequency)
{
	// Detach the pin
	ledcDetachPin(servoPin);
	// Configure the LEDC PWM channel with the new frequency
	laser_setup_channel(servoPin, ledChannel, frequency, resolution);
}

// Custom function accessible by the API
// returns json {"laser":{..}}  as qid
cJSON *laser_get(cJSON *ob)
{
	/*
	 {"task":"/laser_get"}
	 {"task":"/laser_get", "qid":1}
	 returns {"laser": 	{"LASER1pin":1,
						"LASER2pin":2,
						"LASER3pin":3,
						"LASER1val":0,
						"LASER2val":0,
						"LASER3val":0}} */
	(void)ob;
	cJSON *j = cJSON_CreateObject();
	cJSON *ls = cJSON_CreateObject();
	cJSON_AddItemToObject(j, "laser", ls);
	cjson_tool_set_int(ls, "LASER1pin", pinConfig.LASER_1);
	cjson_tool_set_int(ls, "LASER2pin", pinConfig.LASER_2);
	cjson_tool_set_int(ls, "LASER3pin", pinConfig.LASER_3);
	cjson_tool_set_int(ls, "LASER1val", laser_get_val(1));
	cjson_tool_set_int(ls, "LASER2val", laser_get_val(2));
	cjson_tool_set_int(ls, "LASER3val", laser_get_val(3));

	return j;
}

void laser_init(void)
{
	log_i("Setting Up LASERs");

	// Initialize PWM parameters from PinConfig (allows per-board overrides)
	pwm_frequency = pinConfig.LASER_PWM_FREQUENCY;
	pwm_resolution = pinConfig.LASER_PWM_RESOLUTION;
	pwm_max = (long)1 << pwm_resolution;
	log_i("Laser PWM: %d Hz, %d-bit resolution (%ld steps)", pwm_frequency, pwm_resolution, pwm_max);

	// Setup all lasers using array iteration
	// delay(5000);
	for (int i = 0; i < LASER_MAX_LASERS; i++)
	{
		//          0 = LOCAL (on-board GPIO),
		//          1 = REMOTE (forward over CAN to the CAN_ID_* node),
		//          2 = OFF (disabled)
		// Check for local vs remote routing through the ROUTE_LASER table (0 local, 1 remote, 2 off).
		// ROUTE_LASER is sized [4] (lasers 0-3); laser id 4 uses the separate ROUTE_LASER_4
		// entry and the reserved ids 5..LASER_MAX_LASERS-1 have no routing (treated as OFF).
		const int8_t route = (i < 4) ? pinConfig.ROUTE_LASER[i]
			: (i == 4) ? pinConfig.ROUTE_LASER_4 : 2;
		if (route == 2) // OFF
		{
			log_i("LASERid %i is set to OFF in ROUTE_LASER, skipping setup", i);
			continue;
		}
		else if (route == 1) // REMOTE
		{
			log_i("LASERid %i is set to REMOTE in ROUTE_LASER, skipping local setup", i);
			continue;
		}
		else
		{
			log_i("LASERid %i is set to LOCAL in ROUTE_LASER, proceeding with setup", i);
			// Proceed with local setup
		}

		int laserPin = laser_get_pin(i);

		// Skip if pin is not configured
		if (laserPin <= 0)
		{
			log_i("LASERid %i has no valid pin configured, skipping setup", i);
			continue;
		}

		const char *laserName = (i == 0) ? "Heating Unit" : "Laser";
		log_i("%s ID %i, pin: %i", laserName, i, laserPin);
		// BEWARE: THE LED ON THE ILLU BOARD NEEDS 12V TO LIGHT UP!

		pinMode(laserPin, OUTPUT);
		digitalWrite(laserPin, LOW);
		laser_setup_channel(laserPin, laser_get_pwm_channel(i), pwm_frequency, pwm_resolution);
		laser_set_val(i, 0);
	}
	log_i("LASER setup complete");
}

void laser_loop(void)
{
	// Check for pending laser value updates and send them
	for (int i = 0; i < LASER_MAX_LASERS; i++)
	{
		if (laserValuePending[i])
		{
			laser_send_value(i, pendingQid[i]);
			laserValuePending[i] = false; // Clear the flag after sending
			pendingQid[i] = 0; // Reset qid
		}
	}

	// Process hold actions for continuous laser adjustment
	laser_process_hold_actions();

	// Despeckle loop - iterate through all lasers
	for (int i = 0; i < LASER_MAX_LASERS; i++)
	{
		int laserPin = laser_get_pin(i);
		if (laser_despeckle_arr[i] > 0 && laser_val_arr[i] > 0 && laserPin > 0)
		{
			laser_despeckle(laser_despeckle_arr[i], i, laser_despeckle_period_arr[i]);
		}
	}
}

void laser_cross_changed_event(int pressed)
{
	if (pressed)
	{
		unsigned long currentTime = millis();

		// Debounce check - ignore if too soon after last event
		if (currentTime - crossLastEventTime < BUTTON_DEBOUNCE_TIME)
		{
			return;
		}

		crossLastEventTime = currentTime;

		// Toggle Laser 4 state
		laser4ToggleState = !laser4ToggleState;

		log_i("Cross pressed - Laser 4 toggle to %s", laser4ToggleState ? "MAX (10000)" : "MIN (0)");

		// Set Laser 4 to MAX or MIN through the router.
		cJSON *doc = cJSON_CreateObject();
		cJSON_AddNumberToObject(doc, "LASERid", 4);
		cJSON_AddNumberToObject(doc, "LASERval", laser4ToggleState ? 10000 : 0);
		cJSON *resp = device_router_handle_laser_act(doc);
		if (resp)
			cJSON_Delete(resp);
		cJSON_Delete(doc);
	}
}
