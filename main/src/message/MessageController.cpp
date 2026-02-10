#include <PinConfig.h>

#include "MessageController.h"
#include "cJsonTool.h"
#include "Arduino.h"
#include "JsonKeys.h"
#include <Preferences.h>
#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#include "../motor/MotorTypes.h"
#endif
#ifdef HOME_MOTOR
#include "../home/HomeMotor.h"
#endif

namespace MessageController
{
	const char * TAG = "MessageController";
	
	// Preferences for reading stored home parameters
	static Preferences homePrefs;

	void setup()
	{
		log_d("Setup MessageController");
	}

	// Helper function to start homing with stored preferences
	void startHomingWithStoredParams(int axis)
	{
		#ifdef HOME_MOTOR
		// Read stored homing parameters from preferences
		homePrefs.begin("home", true); // read-only mode
		int homeTimeout = homePrefs.getInt(("to_" + String(axis)).c_str(), 20000); // default 20s timeout
		int homeSpeed = homePrefs.getInt(("hs_" + String(axis)).c_str(), 5000);    // default speed
		int homeMaxspeed = homePrefs.getInt(("hms_" + String(axis)).c_str(), 5000); // default maxspeed
		int homeDirection = homePrefs.getInt(("hd_" + String(axis)).c_str(), 1);    // default direction
		int homeEndStopPolarity = homePrefs.getInt(("hep_" + String(axis)).c_str(), 0); // default polarity
		homePrefs.end();
		
		log_i("Starting homing for axis %d with stored params: timeout=%d, speed=%d, maxspeed=%d, dir=%d, polarity=%d",
			  axis, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity);
		
		// Start homing with the stored parameters
		HomeMotor::startHome(axis, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, -99);
		
		// Send feedback message (key=100+axis indicates homing started)
		sendMesageSerial(100 + axis, 1);
		#else
		log_w("HOME_MOTOR not defined, cannot perform homing for axis %d", axis);
		#endif
	}
	
	// Loop function - currently not used as long-press is handled in main.cpp
	void loop()
	{
		// Long-press handling is done in main.cpp checkBtButtonLongPress()
	}

    void triangle_changed_event(int pressed)
    {
		// Triangle button: Short press = Emergency release of hard-limit blocked motors
		// Long press handling is done in main.cpp
		
		if (pressed)
		{
			// Perform emergency release of hard-limit blocked motors
			#ifdef MOTOR_CONTROLLER
			log_i("Triangle pressed - Releasing all hard-limit blocked motors");
			int releasedCount = 0;
			
			// Check all motor axes and release any that are blocked
			for (int axis = 0; axis < MOTOR_AXIS_COUNT; axis++)
			{
				if (FocusMotor::getData()[axis]->hardLimitTriggered)
				{
					log_i("Releasing hard-limit on axis %d via homing reset", axis);
					
					#ifdef HOME_MOTOR
					// Use homing event with timeout=0 to clear hard limit
					HomeMotor::startHome(axis, 1, 0, 0, 1, 0, -99);
					HomeMotor::getHomeData()[axis]->homeIsActive = false;
					FocusMotor::getData()[axis]->isHoming = false;
					#endif
					
					// Clear local state
					FocusMotor::clearHardLimitTriggered(axis);
					FocusMotor::setPosition(static_cast<Stepper>(axis), 0);
					FocusMotor::sendMotorPos(axis, 0);
					releasedCount++;
				}
			}
			
			if (releasedCount > 0)
			{
				sendMesageSerial(99, releasedCount);
				log_i("Released %d blocked motor(s)", releasedCount);
			}
			#endif
			
			// Send message for backwards compatibility
			sendMesageSerial(1, 1);
		}
    }

	void cross_changed_event(int pressed)
    {
		// Cross button: Short press action
		// Long press handling is done in main.cpp
		if (pressed)
		{
			log_i("Cross short press");
			sendMesageSerial(2, 1);
		}
    }

	void circle_changed_event(int pressed)
    {
		// Circle button: Short press action
		// Long press handling is done in main.cpp
		if (pressed)
		{
			log_i("Circle short press");
			sendMesageSerial(3, 1);
		}
    }

	void square_changed_event(int pressed)
    {
		// Square button: Short press action
		// Long press handling is done in main.cpp
		if (pressed)
		{
			log_i("Square short press");
			sendMesageSerial(4, 1);
		}
    }

    // Custom function accessible by the API
	
	int act(cJSON *ob)
	{	// {"message":{"key":"message","value":2}} or 
		// {"task":"/message_act", "message":{"key":"message","value":2}}
		/*
		data lookup:
		{
			key 	|	 meaning 		|	value
			--------------------------------
			1		|	snap image		|	0
			2		|	exposure time	|	0....1000000
			3		|	gain			|	0...100

		*/
	ESP_LOGI(TAG, "act");
	cJSON *message = cJSON_GetObjectItem(ob, "message");
	int qid = 0;
	if (message != NULL){
    	qid = cJsonTool::getJsonInt(message, "qid");
		int key = cJsonTool::getJsonInt(message, "key");
		int value = cJsonTool::getJsonInt(message, "value");
		// now we print this data over serial	
		sendMesageSerial(key, value);
	}
		return qid;
	}

	// Custom function accessible by the API
	// returns json {"Message":{..}}  as qid
	cJSON *get(cJSON *ob)
	{
		cJSON *j = cJSON_CreateObject();
		cJSON *ld = cJSON_CreateObject();
		cJSON_AddItemToObject(j, "message", ld);
		cJsonTool::setJsonInt(ld, "data", 1);
		return j;
	}

	// home done returns
	//{"message":{...}} thats the qid
	void sendMesageSerial(int key, int value)
	{
		// send home done to client
		log_i("key: %i, value: %i", key, value);
		cJSON *json = cJSON_CreateObject();
		cJSON *message = cJSON_CreateObject();
		cJSON_AddItemToObject(json, "message", message);
		cJSON *jsonkey = cJSON_CreateNumber(key);
		cJSON *jsonvalue = cJSON_CreateNumber(value);
		cJSON_AddItemToObject(message, "key", jsonkey);
		cJSON_AddItemToObject(message, "data", jsonvalue);
		Serial.println("++");
		char *ret = cJSON_PrintUnformatted(json);
		cJSON_Delete(json);  // Only delete the top-level object
		Serial.println(ret);
		free(ret);
		Serial.println("--");

	}
}