#include <PinConfig.h>
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
		int qid = cJsonTool::getJsonInt(j, "qid");
		if (home != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItem(home, key_steppers);
			if (stprs != NULL)
			{

				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				getData()[s]->data[s]->isforever = true;
				getData()[s]->data[s]->speed = hdata[s]->homeDirection * abs(hdata[s]->homeSpeed);
				getData()[s]->data[s]->maxspeed = hdata[s]->homeDirection * abs(hdata[s]->homeMaxspeed);
				getData()[s]->startStepper(s);
				if (s == Stepper::Z and (getData()[s]->isDualAxisZ == true))
				{
					Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					hdata[s]->homeTimeout = getJsonInt(stp, key_home_timeout);
					hdata[s]->homeSpeed = getJsonInt(stp, key_home_speed);
					hdata[s]->homeMaxspeed = getJsonInt(stp, key_home_maxspeed);
					hdata[s]->homeDirection = getJsonInt(stp, key_home_direction);
					hdata[s]->homeEndStopPolarity = getJsonInt(stp, key_home_endstoppolarity);
					hdata[s]->qid = qid;
					hdata[s]->homeInEndposReleaseMode = false;
					// grab current time
					hdata[s]->homeTimeStarted = millis();
					hdata[s]->homeIsActive = true;

					// trigger go home by starting the motor in the right direction
					// ensure direction is either 1 or -1
					if (hdata[s]->homeDirection >= 0)
					{
						hdata[s]->homeDirection = 1;
					}
					else
					{
						hdata[s]->homeDirection = -1;
					}
					// ensure endstoppolarity is either 0 or 1
					if (hdata[s]->homeEndStopPolarity > 0)
					{
						hdata[s]->homeEndStopPolarity = 1;
					}
					else
					{
						hdata[s]->homeEndStopPolarity = 0;
					}
					// trigger go home by starting the motor in the right direction
					#ifdef MOTOR_CONTROLLER
					getData()[s]->data[s]->isforever = true;
					getData()[s]->data[s]->speed = hdata[s]->homeDirection * abs(hdata[s]->homeSpeed);
					getData()[s]->data[s]->maxspeed = hdata[s]->homeDirection * abs(hdata[s]->homeMaxspeed);
					getData()[s]->startStepper(s);
					if (s == Stepper::Z and pinConfig.isDualAxisZ)
					{
						// we may have a dual axis so we would need to start A too
						log_i("Starting A too");
						getData()[s]->data[Stepper::A]->isforever = true;
						getData()[s]->data[Stepper::A]->speed = hdata[s]->homeDirection * abs(hdata[s]->homeSpeed);
						getData()[s]->data[Stepper::A]->maxspeed = hdata[s]->homeDirection * abs(hdata[s]->homeMaxspeed);
						getData()[s]->startStepper(Stepper::A);
					}
					#endif
					// now we will go into loop and need to stop once the button is hit or timeout is reached
					log_i("Home Data Motor  Axis: %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i, homeEndosReleaseMode %i, endstop polarity %i",
						  s, hdata[s]->homeTimeout, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection * hdata[s]->homeSpeed,
						  hdata[s]->homeDirection, hdata[s]->homeTimeStarted, hdata[s]->homeInEndposReleaseMode, hdata[s]->homeEndStopPolarity);
				}
			}
		}
		return qid;
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
void HomeMotor::checkAndProcessHome(Stepper s, int digitalin_val)
{

#ifdef MOTOR_CONTROLLER

	// if we hit the endstop, reverse direction
	if (hdata[s]->homeIsActive && (abs(hdata[s]->homeEndStopPolarity - digitalin_val) ||
		hdata[s]->homeTimeStarted + hdata[s]->homeTimeout < millis()) &&
		hdata[s]->homeInEndposReleaseMode == 0)
	{
		log_i("Home Motor %i in endpos release mode %i", s, hdata[s]->homeInEndposReleaseMode);
		// homeInEndposReleaseMode = 0 means we are not in endpos release mode
		// homeInEndposReleaseMode = 1 means we are in endpos release mode
		// homeInEndposReleaseMode = 2 means we are done
		// reverse direction to release endstops
		getData()[s]->stopStepper(s);
		if (s == Stepper::Z and (pinConfig.isDualAxisZ==true))
		{
			// we may have a dual axis so we would need to start A too
			getData()[s]->stopStepper(Stepper::A);
		}
		hdata[s]->homeInEndposReleaseMode = 1;
		getData()[s]->data[s]->speed = -hdata[s]->homeDirection * abs(hdata[s]->homeSpeed);
		getData()[s]->data[s]->isforever = true;
		getData()[s]->data[s]->acceleration = MAX_ACCELERATION_A;
		delay(50);
		log_i("Motor speed was %i and will be %i", getData()[s]->data[s]->speed , -getData()[s]->data[s]->speed );

	}
	else if (hdata[s]->homeIsActive && hdata[s]->homeInEndposReleaseMode == 1)
	{
		log_i("Home Motor %i in endpos release mode  %i", s, hdata[s]->homeInEndposReleaseMode);
		getData()[s]->startStepper(s);

		if (s == Stepper::Z and (motor->isDualAxisZ == true))
		{
			// we may have a dual axis so we would need to start A too
			getData()[s]->data[Stepper::A]->speed = -getData()[s]->data[Stepper::A]->speed ;
			getData()[s]->data[Stepper::A]->isforever = true;
			getData()[s]->startStepper(Stepper::A);
		}
		hdata[s]->homeInEndposReleaseMode = 2;
	}
	// if we are in endpos release mode and the endstop is released, stop the motor - or if timeout is reached
	else if (hdata[s]->homeIsActive && hdata[s]->homeInEndposReleaseMode == 2 &&
			 (!abs(hdata[s]->homeEndStopPolarity - digitalin_val) || hdata[s]->homeTimeStarted + hdata[s]->homeTimeout < millis()))
	{
		log_i("Home Motor %i in endpos release mode %i", s, hdata[s]->homeInEndposReleaseMode);
		getData()[s]->stopStepper(s);
		getData()[s]->setPosition(s, 0);
		if (s == Stepper::Z and (pinConfig.isDualAxisZ==true))
		{
			// we may have a dual axis so we would need to start A too
			getData()[s]->stopStepper(Stepper::A);
			getData()[s]->setPosition(Stepper::A, 0);
		}
		getData()[s]->data[s]->isforever = false;
		log_i("Home Motor X done");
		sendHomeDone(s);
		if (s == Stepper::A and (motor->isDualAxisZ == true))
		{
			// we may have a dual axis so we would need to start A too
			hdata[Stepper::A]->homeIsActive = false;
			getData()[s]->setPosition(Stepper::A, 0);
			getData()[s]->stopStepper(Stepper::A);
			getData()[s]->data[Stepper::A]->isforever = false;
			log_i("Home Motor A done");
			sendHomeDone(Stepper::A);
		}
		hdata[s]->homeIsActive = false;
		hdata[s]->homeInEndposReleaseMode = false;
	}
#endif
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
#if defined MOTOR_CONTROLLER && defined DIGITAL_IN_CONTROLLER
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
