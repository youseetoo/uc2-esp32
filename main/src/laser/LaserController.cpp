#include <PinConfig.h>
#include "LaserController.h"
#include "cJsonTool.h"
#include "JsonKeys.h"
#include "../state/State.h"
#include "../serial/SerialProcess.h"
#ifdef WIFI
#include "../wifi/WifiController.h"
#endif
#ifdef I2C_LASER 
#include "../i2c/i2c_master.h"
#endif
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif
namespace LaserController
{
	// Flags to track pending laser value updates that need to be sent
	static bool laserValuePending[5] = {false, false, false, false, false}; // For LASER IDs 0-4
	static int pendingQid[5] = {0, 0, 0, 0, 0}; // Store qid for each pending update

	void LASER_despeckle(int LASERdespeckle, int LASERid, int LASERperiod)
	{
		log_e("LASERdespeckle %i, LASERid %i, LASERperiod %i", LASERdespeckle, LASERid, LASERperiod);
		int LASER_val_wiggle = 0;
		int PWM_CHANNEL_LASER = 0;
		if (LASERid == 1)
		{
			LASER_val_wiggle = LASER_val_1;
			PWM_CHANNEL_LASER = PWM_CHANNEL_LASER_1;
		}
		else if (LASERid == 2)
		{
			LASER_val_wiggle = LASER_val_2;
			PWM_CHANNEL_LASER = PWM_CHANNEL_LASER_2;
		}
		else if (LASERid == 3)
		{
			LASER_val_wiggle = LASER_val_3;
			PWM_CHANNEL_LASER = PWM_CHANNEL_LASER_3;
		}
		// add random number to current value to let it oscliate
		int32_t laserwiggle = random(-LASERdespeckle, LASERdespeckle);
		LASER_val_wiggle += laserwiggle;
		if (LASER_val_wiggle > pwm_max)
			LASER_val_wiggle -= (2 * abs(laserwiggle));
		if (LASER_val_wiggle < 0)
			LASER_val_wiggle += (2 * abs(laserwiggle));

		log_d("%i", LASERid);
		log_d("%i", LASER_val_wiggle);

		setPWM(LASER_val_wiggle, PWM_CHANNEL_LASER);
		delay(LASERperiod);
	}

	void setPWM(int pwmValue, int pwmChannel)
	{
		// sets the PWM value for the given channel
		log_i("Setting PWM value %i on channel %i", pwmValue, pwmChannel);
		ledcWrite(pwmChannel, pwmValue);
	}

	// Custom function accessible by the API
	int act(cJSON *ob)
	{
		// JSON String
		// {"task":"/laser_act", "LASERid":1, "LASERval":1000}, "LASERdespeckle":0, "LASERdespecklePeriod":0}
		// {"task":"/laser_act", "LASERid":2, "LASERval":1000}
		// {"task":"/laser_act", "LASERid":2, "LASERval":10000}
		State::setBusy(true);
		int qid = cJsonTool::getJsonInt(ob, "qid");
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
		LASERid = cJsonTool::getJsonInt(ob, "LASERid");
		LASERval = cJsonTool::getJsonInt(ob, "LASERval");
		LASERdespeckle = cJsonTool::getJsonInt(ob, "LASERdespeckle");
		LASERdespecklePeriod = cJsonTool::getJsonInt(ob, "LASERdespecklePeriod");


		LaserData laserData;
		laserData.LASERid = LASERid;
		laserData.LASERval = LASERval;
		laserData.LASERdespeckle = LASERdespeckle;
		laserData.LASERdespecklePeriod = LASERdespecklePeriod;
		#ifdef I2C_LASER
		i2c_master::sendLaserDataI2C(laserData, LASERid);
		return qid; 
		#elif defined CAN_CONTROLLER && not defined(CAN_SLAVE_LASER)
		can_controller::sendLaserDataToCANDriver(laserData);
		return qid;
		#else

		// debugging
		log_i("LaserID %i, LaserVal %i, LaserDespeckle %i, LaserDespecklePeriod %i", LASERid, LASERval, LASERdespeckle, LASERdespecklePeriod);

		/*
		Set Laser PWM Frequency
		*/
		if (setPWMFreq != NULL and isServo == false)
		{ // {"task":"/laser_act", "LASERid":1 ,"LASERFreq":50, "LASERval":1000, "qid":1}
			pwm_frequency = setPWMFreq->valueint;
			log_i("Setting PWM frequency to %i", pwm_frequency);
			if (LASERid == 1 && pinConfig.LASER_1 != 0)
			{
				setupLaser(pinConfig.LASER_1, PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 2 && pinConfig.LASER_2 != 0)
			{
				setupLaser(pinConfig.LASER_2, PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 3 && pinConfig.LASER_3 != 0)
			{
				setupLaser(pinConfig.LASER_3, PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
			}
		}

		/*
		Set Laser PWM REsolution
		*/
		if (setPWMRes != NULL and isServo == false)
		{ // {"task":"/laser_act", "LASERid":2 ,"LASERRes":16} // for servo
			log_i("Setting PWM frequency to %i", pwm_frequency);
			if (LASERid == 1 && pinConfig.LASER_1 != 0)
			{
				setupLaser(pinConfig.LASER_1, PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 2 && pinConfig.LASER_2 != 0)
			{
				setupLaser(pinConfig.LASER_2, PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 3 && pinConfig.LASER_3 != 0)
			{
				setupLaser(pinConfig.LASER_3, PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
			}
		}

		// action LASER 1
		if (LASERid == 1 && pinConfig.LASER_1 >= 0 && hasLASERval)
		{
			LASER_val_1 = LASERval;
			LASER_despeckle_1 = LASERdespeckle;
			LASER_despeckle_period_1 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":1 ,"LASERval":99, "servo":1, "qid":1}
				// {"task":"/laser_act", "LASERid":2 ,"LASERval":90, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_1, pwm_resolution, PWM_CHANNEL_LASER_1, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_1, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				setLaserVal(LASERid, LASER_val_1, qid);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		// action LASER 2
		else if (LASERid == 2 && pinConfig.LASER_2 >= 0 && hasLASERval)
		{
			LASER_val_2 = LASERval;
			LASER_despeckle_2 = LASERdespeckle;
			LASER_despeckle_period_2 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":2 ,"LASERval":50, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_2, pwm_resolution, PWM_CHANNEL_LASER_2, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_2, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				setLaserVal(LASERid, LASER_val_2, qid);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		// action LASER 3
		else if (LASERid == 3 && pinConfig.LASER_3 >= 0 && hasLASERval)
		{
			LASER_val_3 = LASERval;
			LASER_despeckle_3 = LASERdespeckle;
			LASER_despeckle_period_3 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":3 ,"LASERval":50, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_3, pwm_resolution, PWM_CHANNEL_LASER_3, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_3, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				setLaserVal(LASERid, LASER_val_3, qid);
			}

			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		// action LASER 4
		else if (LASERid == 4 && pinConfig.LASER_4 >= 0 && hasLASERval)
		{
			LASER_val_4 = LASERval;
			LASER_despeckle_4 = LASERdespeckle;
			LASER_despeckle_period_4 = LASERdespecklePeriod;
			if (isServo)
			{
				pwm_frequency = 50;
				pwm_resolution = 16;
				configurePWM(pinConfig.LASER_4, pwm_resolution, PWM_CHANNEL_LASER_4, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_4, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				setLaserVal(LASERid, LASER_val_4, qid);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		// action LASER 0
		else if (LASERid == 0 && pinConfig.LASER_0 >= 0 && hasLASERval)
		{
			// action LASER 0
			// {"task":"/laser_act", "LASERid":0 ,"LASERval":50, "qid":1}
			LASER_val_0 = LASERval;
			LASER_despeckle_0 = LASERdespeckle;
			LASER_despeckle_period_0 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":0 ,"LASERval":50, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_0, pwm_frequency, PWM_CHANNEL_LASER_0, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_0, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				setLaserVal(LASERid, LASER_val_0, qid);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		else
		{
			State::setBusy(false);
			return 0;
		}
		#endif
	}

		void applyLaserValue(const LaserData& laserData)
	{
		#ifdef I2C_LASER
			i2c_master::sendLaserDataI2C(laserData, laserData.LASERid);
		#elif defined(CAN_CONTROLLER) && !defined(CAN_SLAVE_LASER)
			can_controller::sendLaserDataToCANDriver(laserData);
		#else
			int pwmChannel = -1;
			int value = laserData.LASERval;
			switch (laserData.LASERid)
			{
				case 0: LASER_val_0 = value; pwmChannel = PWM_CHANNEL_LASER_0; break;
				case 1: LASER_val_1 = value; pwmChannel = PWM_CHANNEL_LASER_1; break;
				case 2: LASER_val_2 = value; pwmChannel = PWM_CHANNEL_LASER_2; break;
				case 3: LASER_val_3 = value; pwmChannel = PWM_CHANNEL_LASER_3; break;
				case 4: LASER_val_4 = value; pwmChannel = PWM_CHANNEL_LASER_4; break;
				default: log_w("Invalid LASERid: %d", laserData.LASERid); return;
			}

			if (pwmChannel != -1)
			{
				setPWM(value, pwmChannel);
			}
		#endif
	}


	bool setLaserVal(int LASERid, int LASERval, int qid)
	{
		log_i("Setting Laser Value: LASERid %i, LASERval %i, qid %i", LASERid, LASERval, qid);
		
		// Store qid for this laser
		if (LASERid >= 0 && LASERid <= 4) {
			pendingQid[LASERid] = qid;
		}
		
		#ifdef I2C_LASER
		LaserData laserData;
		laserData.LASERid = LASERid;
		laserData.LASERval = LASERval;
		laserData.LASERdespeckle = 0;
		laserData.LASERdespecklePeriod = 0;
		i2c_master::sendLaserDataI2C(laserData, LASERid);
		
		// Set flag to send update in next loop cycle
		if (LASERid >= 0 && LASERid <= 4) {
			laserValuePending[LASERid] = true;
		}
		return true;
		#elif defined CAN_CONTROLLER && not defined(CAN_SLAVE_LASER)
		LaserData laserData;
		laserData.LASERid = LASERid;
		laserData.LASERval = LASERval;
		laserData.LASERdespeckle = 0;
		laserData.LASERdespecklePeriod = 0;
		can_controller::sendLaserDataToCANDriver(laserData);
		
		// Set flag to send update in next loop cycle
		if (LASERid >= 0 && LASERid <= 4) {
			laserValuePending[LASERid] = true;
		}
		return true;
		#else
		if (LASERid == 0 && pinConfig.LASER_0 != 0)
		{
			LASER_val_0 = LASERval;
			setPWM(LASER_val_0, PWM_CHANNEL_LASER_0);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			
			// Set flag to send update in next loop cycle
			laserValuePending[0] = true;
			return true;
		}
		else if (LASERid == 1 && pinConfig.LASER_1 != 0)
		{
			LASER_val_1 = LASERval;
			setPWM(LASER_val_1, PWM_CHANNEL_LASER_1);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			
			// Set flag to send update in next loop cycle
			laserValuePending[1] = true;
			return true;
		}
		else if (LASERid == 2 && pinConfig.LASER_2 != 0)
		{
			LASER_val_2 = LASERval;
			setPWM(LASER_val_2, PWM_CHANNEL_LASER_2);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			
			// Set flag to send update in next loop cycle
			laserValuePending[2] = true;
			return true;
		}
		else if (LASERid == 3 && pinConfig.LASER_3 != 0)
		{
			LASER_val_3 = LASERval;
			setPWM(LASER_val_3, PWM_CHANNEL_LASER_3);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			
			// Set flag to send update in next loop cycle
			laserValuePending[3] = true;
			return true;
		}
		else if (LASERid == 4 && pinConfig.LASER_4 != 0)
		{
			LASER_val_4 = LASERval;
			setPWM(LASER_val_4, PWM_CHANNEL_LASER_4);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			
			// Set flag to send update in next loop cycle
			laserValuePending[4] = true;
			return true;
		}
		else
		{
			return false;
		}
		#endif
	}

	int getLaserVal(int LASERid)
	{
		int laserVal = 0;
		if (LASERid == 0)
		{
			laserVal = LASER_val_0;
		}
		else if (LASERid == 1)
		{
			laserVal = LASER_val_1;
		}
		else if (LASERid == 2)
		{
			laserVal = LASER_val_2;
		}
		else if (LASERid == 3)
		{
			laserVal = LASER_val_3;
		}
		else if (LASERid == 4)
		{
			laserVal = LASER_val_4;
		}
		else
		{
			laserVal = 0;
		}
		log_i("LASERid %i, LASERval %i", LASERid, laserVal);
		return laserVal;
	}

	void sendLaserValue(int LASERid, int qid)
	{
		// Send laser value update similar to sendMotorPos pattern
		// JSON format: {"laser": {"LASERid": X, "LASERval": Y}, "qid": Z}
		
		log_i("Sending laser value update: LASERid %i, LASERval %i, qid %i", 
		      LASERid, getLaserVal(LASERid), qid);
		
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
		cJSON_AddNumberToObject(laserObj, "LASERval", getLaserVal(LASERid));
		cJSON_AddNumberToObject(laserObj, "isDone", true); // Laser is immediately done after setting
		
		if (qid != 0)
		{
			cJSON_AddNumberToObject(root, "qid", qid);
		}

#ifdef WIFI
		WifiController::sendJsonWebSocketMsg(root);
#endif

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
		SerialProcess::safeSendJsonString(jsonString);
		
		// Free the serialized string
		free(jsonString);
		jsonString = NULL;
		
		log_d("Laser value update sent successfully for LASERid %i", LASERid);
	}

	void setupLaser(int laser_pin, int pwm_chan, int pwm_freq, int pwm_res)
	{
		// Setting up the differen PWM channels for the laser
		log_i("Setting up Laser with PWM Channel %i, PWM Frequency %i, PWM Resolution %i", pwm_chan, pwm_freq, pwm_res);
		ledcSetup(pwm_chan, pwm_freq, pwm_res);
		ledcAttachPin(laser_pin, pwm_chan);
	}

	LaserData getLaserData(){
		LaserData laserData;
		laserData.LASERid = 1;
		laserData.LASERval = LASER_val_1;
		laserData.LASERdespeckle = LASER_despeckle_1;
		laserData.LASERdespecklePeriod = LASER_despeckle_period_1;
		return laserData;
	}

	bool laser_on = false;
	bool laser2_on = false;
	
	// Variables for hold/release detection
	static unsigned long pressStartTime[4] = {0, 0, 0, 0}; // UP, DOWN, RIGHT, LEFT
	static bool isPressed[4] = {false, false, false, false};
	static bool isHolding[4] = {false, false, false, false};
	static unsigned long lastHoldAction[4] = {0, 0, 0, 0};
	static const unsigned long HOLD_THRESHOLD = 500; // ms to detect hold
	static const unsigned long HOLD_INCREMENT_INTERVAL = 200; // ms between increments
	
	// Cross button toggle for Laser 4
	static unsigned long crossLastEventTime = 0;
	static const unsigned long BUTTON_DEBOUNCE_TIME = 300; // ms to prevent multiple toggles
	static bool laser4ToggleState = false; // false = off/min, true = on/max
	
	void dpad_changed_event(Dpad::Direction pressed)
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
		
		if (pressed == Dpad::Direction::up)
		{
			if (!isPressed[0]) // Button press started
			{
				isPressed[0] = true;
				pressStartTime[0] = currentTime;
				isHolding[0] = false;
			}
		}
		else if (pressed == Dpad::Direction::down)
		{
			if (!isPressed[1]) // Button press started
			{
				isPressed[1] = true;
				pressStartTime[1] = currentTime;
				isHolding[1] = false;
			}
		}
		else if (pressed == Dpad::Direction::right)
		{
			if (!isPressed[2]) // Button press started
			{
				isPressed[2] = true;
				pressStartTime[2] = currentTime;
				isHolding[2] = false;
			}
		}
		else if (pressed == Dpad::Direction::left)
		{
			if (!isPressed[3]) // Button press started
			{
				isPressed[3] = true;
				pressStartTime[3] = currentTime;
				isHolding[3] = false;
			}
		}
		else if (pressed == Dpad::Direction::none)
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
						handleShortClick(i);
					}
					isHolding[i] = false;
				}
			}
		}
	}
	
	void handleShortClick(int direction)
	{
		// 0=UP, 1=DOWN, 2=RIGHT, 3=LEFT
		if (direction == 0 || direction == 1) // UP or DOWN - Laser 1
		{
			if (laser_on)
			{
				log_i("Short click - Laser 1 OFF");
				LaserController::setLaserVal(1, 0);
				laser_on = false;
			}
			else
			{
				log_i("Short click - Laser 1 ON");
				LaserController::setLaserVal(1, 10000);
				laser_on = true;
			}
		}
		else if (direction == 2 || direction == 3) // RIGHT or LEFT - Laser 2
		{
			if (laser2_on)
			{
				log_i("Short click - Laser 2 OFF");
				LaserController::setLaserVal(2, 0);
				laser2_on = false;
			}
			else
			{
				log_i("Short click - Laser 2 ON");
				LaserController::setLaserVal(2, 10000);
				laser2_on = true;
			}
		}
	}
	
	// Call this from loop() to handle hold actions
	void processHoldActions()
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
				executeHoldAction(i);
				lastHoldAction[i] = currentTime;
			}
		}
	}
	
	void executeHoldAction(int direction)
	{
		// 0=UP, 1=DOWN, 2=RIGHT, 3=LEFT
		int incrementValue = 100;
		int maxValue = 20000;
		if (direction == 0) // UP - increment Laser 1
		{
			int currentVal = getLaserVal(1);
			int newVal = std::min(currentVal + incrementValue, maxValue);
			if (newVal != currentVal)
			{
				log_i("Hold UP - incrementing Laser 1: %d -> %d", currentVal, newVal);
				setLaserVal(1, newVal);
				laser_on = (newVal > 0);
			}
		}
		else if (direction == 1) // DOWN - decrement Laser 1
		{
			int currentVal = getLaserVal(1);
			int newVal = std::max(currentVal - incrementValue, 0);
			if (newVal != currentVal)
			{
				log_i("Hold DOWN - decrementing Laser 1: %d -> %d", currentVal, newVal);
				setLaserVal(1, newVal);
				laser_on = (newVal > 0);
			}
		}
		else if (direction == 2) // RIGHT - increment Laser 2
		{
			int currentVal = getLaserVal(2);
			int newVal = std::min(currentVal + incrementValue, maxValue);
			if (newVal != currentVal)
			{
				log_i("Hold RIGHT - incrementing Laser 2: %d -> %d", currentVal, newVal);
				setLaserVal(2, newVal);
				laser2_on = (newVal > 0);
			}
		}
		else if (direction == 3) // LEFT - decrement Laser 2
		{
			int currentVal = getLaserVal(2);
			int newVal = std::max(currentVal - incrementValue, 0);
			if (newVal != currentVal)
			{
				log_i("Hold LEFT - decrementing Laser 2: %d -> %d", currentVal, newVal);
				setLaserVal(2, newVal);
				laser2_on = (newVal > 0);
			}
		}
	}
	
	void moveServo(int ledChannel, int angle, int frequency, int resolution)
	{
		// { "task": "/laser_act", "LASERid": 1, "LASERval": 170, "servo": 1, "qid": 1 }

		// Map the angle to the corresponding pulse width
		int pulseWidth = map(angle, 0, 180, minPulseWidth, maxPulseWidth);
		// Convert the pulse width to a duty cycle value
		int dutyCycle = map(pulseWidth, 0, 1000000 / frequency, 0, (1 << resolution) - 1);
		// Write the duty cycle to the LEDC channel
		ledcWrite(ledChannel, dutyCycle);
		delay(100); // Wait for the servo to reach the position
	}

	void configurePWM(int servoPin, int resolution, int ledChannel, int frequency)
	{
		// Detach the pin
		ledcDetachPin(servoPin);
		// Configure the LEDC PWM channel with the new frequency
		setupLaser(servoPin, ledChannel, frequency, resolution);
	}

	// Custom function accessible by the API
	// returns json {"laser":{..}}  as qid
	cJSON *get(cJSON *ob)
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
		cJSON *j = cJSON_CreateObject();
		cJSON *ls = cJSON_CreateObject();
		cJSON_AddItemToObject(j, key_laser, ls);
		cJsonTool::setJsonInt(ls, "LASER1pin", pinConfig.LASER_1);
		cJsonTool::setJsonInt(ls, "LASER2pin", pinConfig.LASER_2);
		cJsonTool::setJsonInt(ls, "LASER3pin", pinConfig.LASER_3);
		cJsonTool::setJsonInt(ls, "LASER1val", getLaserVal(1));
		cJsonTool::setJsonInt(ls, "LASER2val", getLaserVal(2));
		cJsonTool::setJsonInt(ls, "LASER3val", getLaserVal(3));

		return j;
	}

	void setup()
	{
		log_i("Setting Up LASERs");
		bool testOnBoot = false;
		// Setting up the differen PWM channels for the laser
		log_i("Laser ID 1, pin: %i", pinConfig.LASER_1);
		pinMode(pinConfig.LASER_1, OUTPUT);
		digitalWrite(pinConfig.LASER_1, LOW);
		setupLaser(pinConfig.LASER_1, PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
		if(testOnBoot) setLaserVal(1, 100); // THIS IS ANTI LASERSAFETY!
		delay(10);
		setLaserVal(1, 0);

		log_i("Laser ID 2, pin: %i", pinConfig.LASER_2);
		pinMode(pinConfig.LASER_2, OUTPUT);
		digitalWrite(pinConfig.LASER_2, LOW);
		setupLaser(pinConfig.LASER_2, PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
		if(testOnBoot) setLaserVal(2, 100);
		delay(10);
		setLaserVal(2, 0);

		log_i("Laser ID 3, pin: %i", pinConfig.LASER_3);
		pinMode(pinConfig.LASER_3, OUTPUT);
		digitalWrite(pinConfig.LASER_3, LOW);
		setupLaser(pinConfig.LASER_3, PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
		if(testOnBoot) setLaserVal(3, 100);
		delay(10);
		setLaserVal(3, 0);


		// Setting up the differen PWM channels for the heating unit
		if (pinConfig.LASER_0 > 0)
		{
			log_i("Heating Unit, pin: %i", pinConfig.LASER_0);
			pinMode(pinConfig.LASER_0, OUTPUT);
			digitalWrite(pinConfig.LASER_0, LOW);
			setupLaser(pinConfig.LASER_0, PWM_CHANNEL_LASER_0, pwm_frequency, pwm_resolution);
			if(testOnBoot) setLaserVal(0, 100);
			delay(10);
			setLaserVal(0, 0);
			
		}

		if (pinConfig.LASER_4 > 0)
		{
			log_i("Laser ID 4, pin: %i", pinConfig.LASER_4);
			pinMode(pinConfig.LASER_4, OUTPUT);
			digitalWrite(pinConfig.LASER_4, LOW);
			setupLaser(pinConfig.LASER_4, PWM_CHANNEL_LASER_4, pwm_frequency, pwm_resolution);
			if(testOnBoot) setLaserVal(4, 100);
			delay(10);
			setLaserVal(4, 0);
		}
	}

	void loop()
	{
		// Check for pending laser value updates and send them
		for (int i = 0; i <= 4; i++)
		{
			if (laserValuePending[i])
			{
				sendLaserValue(i, pendingQid[i]);
				laserValuePending[i] = false; // Clear the flag after sending
				pendingQid[i] = 0; // Reset qid
			}
		}
		
		// Process hold actions for continuous laser adjustment
		processHoldActions();

		// attempting to despeckle by wiggeling the temperature-dependent modes of the laser?
		if (LASER_despeckle_1 > 0 && LASER_val_1 > 0 && pinConfig.LASER_1 != 0)
			LASER_despeckle(LASER_despeckle_1, 1, LASER_despeckle_period_1);
		if (LASER_despeckle_2 > 0 && LASER_val_2 > 0 && pinConfig.LASER_2 != 0)
			LASER_despeckle(LASER_despeckle_2, 2, LASER_despeckle_period_2);
		if (LASER_despeckle_3 > 0 && LASER_val_3 > 0 && pinConfig.LASER_3 != 0)
			LASER_despeckle(LASER_despeckle_3, 3, LASER_despeckle_period_3);
	}

	void cross_changed_event(int pressed)
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
			
			if (laser4ToggleState)
			{
				// Turn Laser 4 to MAX
				Serial.println("Cross pressed - Laser 4 MAX");
				setLaserVal(4, 10000);
			}
			else
			{
				// Turn Laser 4 to MIN
				Serial.println("Cross pressed - Laser 4 MIN");
				setLaserVal(4, 0);
			}
		}
	}
}
