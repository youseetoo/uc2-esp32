#include <PinConfig.h>
#ifdef HOME_MOTOR
#include "HomeMotor.h"
#include "FastAccelStepper.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "HardwareSerial.h"
#include "../../cJsonTool.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "../motor/MotorTypes.h"
using namespace FocusMotor;

namespace HomeMotor
{

	void processHomeLoop(void *p)
	{
		
	}

	/*
	Handle REST calls to the HomeMotor module
	*/
	int act(cJSON *j)
	{
		cJSON *home = cJSON_GetObjectItem(j, key_home);
		if (home != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItem(home, key_steppers);
			if (stprs != NULL)
			{
				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					hdata[s]->homeTimeout = cJsonTool::getJsonInt(stp, key_home_timeout);
					hdata[s]->homeSpeed = cJsonTool::getJsonInt(stp, key_home_speed);
					hdata[s]->homeMaxspeed = cJsonTool::getJsonInt(stp, key_home_maxspeed);
					hdata[s]->homeDirection = cJsonTool::getJsonInt(stp, key_home_direction);
					hdata[s]->homeEndposRelease = cJsonTool::getJsonInt(stp, key_home_endposrelease);
					hdata[s]->homeEndStopPolarity = cJsonTool::getJsonInt(stp, key_home_endstoppolarity);

					// grab current time
					hdata[s]->homeTimeStarted = millis();
					hdata[s]->homeIsActive = true;

// trigger go home by starting the motor in the right direction
#ifdef FOCUS_MOTOR
					getData()[s]->isforever = true;
					getData()[s]->speed = hdata[s]->homeDirection * hdata[s]->homeSpeed;
					getData()[s]->maxspeed = hdata[s]->homeDirection * hdata[s]->homeMaxspeed;

					// now we will go into loop and need to stop once the button is hit or timeout is reached
					log_i("Home Data Motor  Axis: %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i, homeEndosRelease %i",
						  s, hdata[s]->homeTimeout, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection, hdata[s]->homeTimeStarted, hdata[s]->homeEndposRelease);

					FocusMotor::startStepper(s);
#endif
				}
			}
		}
		return 1;
	}

	cJSON *get(cJSON *ob)
	{
		log_i("home_get_fct");
		cJSON *doc = cJSON_CreateObject();
		cJSON *home = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, key_home, home);
		cJSON *arr = cJSON_CreateArray();
		cJSON_AddItemToObject(home, key_steppers, arr);

		// add the home data to the json

		for (int i = 0; i < 4; i++)
		{
			cJSON *arritem = cJSON_CreateObject();
			cJSON_AddItemToArray(arr, arritem);
			cJsonTool::setJsonInt(arritem, key_home_timeout, hdata[i]->homeTimeout);
			cJsonTool::setJsonInt(arritem, key_home_speed, hdata[i]->homeSpeed);
			cJsonTool::setJsonInt(arritem, key_home_maxspeed, hdata[i]->homeMaxspeed);
			cJsonTool::setJsonInt(arritem, key_home_direction, hdata[i]->homeDirection);
			cJsonTool::setJsonInt(arritem, key_home_timestarted, hdata[i]->homeTimeStarted);
			cJsonTool::setJsonInt(arritem, key_home_isactive, hdata[i]->homeIsActive);
		}

		log_i("home_get_fct done");
		return doc;
	}

	// home done returns
	//{"home":{...}} thats the qid
	void sendHomeDone(int axis)
	{
		// send home done to client
		cJSON *json = cJSON_CreateObject();
		cJSON *home = cJSON_CreateObject();
		cJSON_AddItemToObject(json, key_home, home);
		cJSON *steppers = cJSON_CreateObject();
		cJSON_AddItemToObject(home, key_steppers, steppers);
		cJSON *axs = cJSON_CreateNumber(axis);
		cJSON *done = cJSON_CreateNumber(true);
		cJSON_AddItemToObject(steppers, "axis", axs);
		cJSON_AddItemToObject(steppers, "isDone", done);
		Serial.println("++");
		char *ret = cJSON_Print(json);
		cJSON_Delete(json);
		Serial.println(ret);
		free(ret);
		Serial.println("--");
	}

	void checkAndProcessHome(Stepper s, int digitalin_val)
	{
		if (hdata[s]->homeIsActive && (abs(hdata[s]->homeEndStopPolarity - digitalin_val) || hdata[s]->homeTimeStarted + hdata[s]->homeTimeout < millis()))
		{
// stopping motor and going reversing direction to release endstops
#ifdef FOCUS_MOTOR
			int speed = getData()[s]->speed;
			FocusMotor::stopStepper(s);
			FocusMotor::setPosition(s, 0);
			// blocks until stepper reached new position wich would be optimal outside of the endstep
			if (speed > 0)
				getData()[s]->targetPosition = -hdata[s]->homeEndposRelease;
			else
				getData()[s]->targetPosition = hdata[s]->homeEndposRelease;
			getData()[s]->absolutePosition = false;
			FocusMotor::startStepper(s);
			// wait until stepper reached new position
			while (FocusMotor::isRunning(s))
				delay(1);
			hdata[s]->homeIsActive = false;
			FocusMotor::setPosition(s, 0);
			FocusMotor::stopStepper(s);
			getData()[s]->isforever = false;
			log_i("Home Motor X done");
			sendHomeDone(s);
#endif
		}
	}
	/*
		get called repeatedly, dont block this
	*/
	void loop()
	{

		// this will be called everytime, so we need to make this optional with a switch
		// get motor and switch instances

// expecting digitalin1 handling endstep for stepper X, digital2 stepper Y, digital3 stepper Z
//  0=A , 1=X, 2=Y , 3=Z
#if defined FOCUS_MOTOR && defined DIGITAL_IN_CONTROLLER
		checkAndProcessHome(Stepper::X, DigitalInController::digitalin_val_1);
		checkAndProcessHome(Stepper::Y, DigitalInController::digitalin_val_2);
		checkAndProcessHome(Stepper::Z, DigitalInController::digitalin_val_3);
#endif
	}

	/*
	not needed all stuff get setup inside motor and digitalin, but must get implemented
	*/
	void setup()
	{
		log_i("HomeMotor setup");
		for (int i = 0; i < 4; i++)
		{
			hdata[i] = new HomeData();
		}
		// xTaskCreate(&processHomeLoop, "home_task", 1024, NULL, 5, NULL);
	}
}
#endif
