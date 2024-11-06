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
#include "../motor/FocusMotor.h"

using namespace FocusMotor;

namespace HomeMotor
{


	/*
	Handle REST calls to the HomeMotor module
	*/
	int act(cJSON *j)
	{
		log_i("home_act_fct");
		// print the json
		char *out = cJSON_Print(j);
		log_i("HomeMotor act %s", out);

		// get the home object that contains multiple axis and parameters
		cJSON *home = cJSON_GetObjectItem(j, key_home);

		// extract the QID from the json
		int qid = cJsonTool::getJsonInt(j, "qid");
#ifdef MOTOR_CONTROLLER
		if (home != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItem(home, key_steppers);
			log_i("HomeMotor act %s", stprs);
			if (stprs != NULL)
			{

				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					log_i("HomeMotor act %s", stp);
					Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					hdata[s]->homeTimeout = cJsonTool::getJsonInt(stp, key_home_timeout);
					hdata[s]->homeSpeed = cJsonTool::getJsonInt(stp, key_home_speed);
					hdata[s]->homeMaxspeed = cJsonTool::getJsonInt(stp, key_home_maxspeed);
					hdata[s]->homeDirection = cJsonTool::getJsonInt(stp, key_home_direction);
					hdata[s]->homeEndStopPolarity = cJsonTool::getJsonInt(stp, key_home_endstoppolarity);
					FocusMotor::isDualAxisZ = cJsonTool::getJsonInt(stp, key_home_isDualAxis);
					hdata[s]->qid = qid;
					hdata[s]->homeInEndposReleaseMode = 0;
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

					FocusMotor::getData()[s]->isforever = true;
					getData()[s]->speed = hdata[s]->homeDirection * abs(hdata[s]->homeSpeed);
					getData()[s]->maxspeed = hdata[s]->homeDirection * abs(hdata[s]->homeMaxspeed);
					getData()[s]->isEnable = 1;
					getData()[s]->isaccelerated = 0;
					FocusMotor::startStepper(s);
					if (s == Stepper::Z and FocusMotor::isDualAxisZ)
					{
						// we may have a dual axis so we would need to start A too
						log_i("Starting A too");
						getData()[Stepper::A]->isforever = true;
						getData()[Stepper::A]->speed = hdata[s]->homeDirection * abs(hdata[s]->homeSpeed);
						getData()[Stepper::A]->maxspeed = hdata[s]->homeDirection * abs(hdata[s]->homeMaxspeed);
						getData()[Stepper::A]->isEnable = 1;
						getData()[Stepper::A]->isaccelerated = 0;
						FocusMotor::startStepper(Stepper::A);
					}

					// now we will go into loop and need to stop once the button is hit or timeout is reached
					log_i("Home Data Motor  Axis: %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i, homeEndosReleaseMode %i, endstop polarity %i",
						  s, hdata[s]->homeTimeout, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection * hdata[s]->homeSpeed,
						  hdata[s]->homeDirection, hdata[s]->homeTimeStarted, hdata[s]->homeInEndposReleaseMode, hdata[s]->homeEndStopPolarity);
				}
			}
		}
#endif
		return qid;
	}

	cJSON *get(cJSON *ob)
	{
		log_i("home_get_fct");
		cJSON *doc = cJSON_CreateObject();
#ifdef MOTOR_CONTROLLER
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
#endif
		return doc;
	}

	// home done returns
	//{"home":{...}}
	void sendHomeDone(int axis)
	{
#ifdef MOTOR_CONTROLLER
		// send home done to client
		cJSON *json = cJSON_CreateObject();
		cJSON *home = cJSON_CreateObject();
		cJSON_AddItemToObject(json, key_home, home);
		cJSON *steppers = cJSON_CreateObject();
		cJSON_AddItemToObject(home, key_steppers, steppers);
		cJSON *axs = cJSON_CreateNumber(axis);
		cJSON *done = cJSON_CreateNumber(true);
		cJSON *pos = cJSON_CreateNumber(FocusMotor::getData()[axis]->currentPosition);
		cJSON_AddItemToObject(steppers, "axis", axs);
		cJSON_AddItemToObject(steppers, "pos", pos);
		cJSON_AddItemToObject(steppers, "isDone", done);
		cJSON_AddItemToObject(json, keyQueueID, cJSON_CreateNumber(hdata[axis]->qid));
		cJsonTool::setJsonInt(json, keyQueueID, hdata[axis]->qid);
		Serial.println("++");
		char *ret = cJSON_PrintUnformatted(json);
		cJSON_Delete(json);
		Serial.println(ret);
		free(ret);
		Serial.println("--");
#endif
	}

	void checkAndProcessHome(Stepper s, int digitalin_val)
	{
#ifdef MOTOR_CONTROLLER

		// log_i("Current STepper %i and digitalin_val %i", s, digitalin_val);
		//  if we hit the endstop or timeout => stop motor and oanch reverse direction mode
		if (hdata[s]->homeIsActive && (abs(hdata[s]->homeEndStopPolarity - digitalin_val) || hdata[s]->homeTimeStarted + hdata[s]->homeTimeout < millis()) &&
			hdata[s]->homeInEndposReleaseMode == 0)
		{
			log_i("Home Motor %i in endpos release mode %i", s, hdata[s]->homeInEndposReleaseMode);
			// homeInEndposReleaseMode = 0 means we are not in endpos release mode
			// homeInEndposReleaseMode = 1 means we are in endpos release mode
			// homeInEndposReleaseMode = 2 means we are done
			// reverse direction to release endstops

			FocusMotor::stopStepper(s);
			FocusMotor::setPosition(s, 0);
			if (s == Stepper::Z and (FocusMotor::isDualAxisZ))
			{
				// we may have a dual axis so we would need to start A too
				FocusMotor::stopStepper(Stepper::A);
				FocusMotor::setPosition(Stepper::A, 0);
			}
			hdata[s]->homeInEndposReleaseMode = 1;
			getData()[s]->speed = -hdata[s]->homeDirection * abs(hdata[s]->homeSpeed);
			getData()[s]->isforever = true;
			getData()[s]->acceleration = MAX_ACCELERATION_A;
			log_i("Motor speed was %i and will be %i", getData()[s]->speed, -getData()[s]->speed);
		}
		else if (hdata[s]->homeIsActive && hdata[s]->homeInEndposReleaseMode == 1)
		{
			// if we are in reverse-direction mode, start motor
			log_i("Home Motor %i in endpos release mode  %i", s, hdata[s]->homeInEndposReleaseMode);
			FocusMotor::startStepper(s);

			if (s == Stepper::Z and (FocusMotor::isDualAxisZ))
			{
				// we may have a dual axis so we would need to start A too
				getData()[Stepper::A]->speed = -getData()[s]->speed;
				getData()[Stepper::A]->isforever = true;
				FocusMotor::startStepper(Stepper::A);
			}
			hdata[s]->homeInEndposReleaseMode = 2;
			hdata[s]->homeTimeStarted = millis();
		}
		// if we are in endpos release mode and the endstop is released, stop the motor - or if timeout is reached (1s)
		else if (hdata[s]->homeIsActive && hdata[s]->homeInEndposReleaseMode == 2 &&
				 (!abs(hdata[s]->homeEndStopPolarity - digitalin_val) || hdata[s]->homeTimeStarted + 1000 < millis()))
		{
			log_i("Home Motor %i in endpos release mode %i", s, hdata[s]->homeInEndposReleaseMode);
			FocusMotor::stopStepper(s);
			FocusMotor::setPosition(s, 0);
			if (s == Stepper::Z and (FocusMotor::isDualAxisZ))
			{
				// we may have a dual axis so we would need to start A too
				FocusMotor::stopStepper(Stepper::A);
				FocusMotor::setPosition(Stepper::A, 0);
				getData()[Stepper::A]->isforever = false;
			}
			getData()[s]->isforever = false;
			hdata[s]->homeInEndposReleaseMode = 3;
		}
		else if (hdata[s]->homeIsActive && hdata[s]->homeInEndposReleaseMode == 3)
		{
			// updating clients
			log_i("Home Motor X done");
			FocusMotor::setPosition(s, 0);
			FocusMotor::sendMotorPos(s, 0);
			sendHomeDone(s);
			hdata[s]->homeIsActive = false;
			hdata[s]->homeInEndposReleaseMode = 0;
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
		checkAndProcessHome(Stepper::X, DigitalInController::getDigitalVal(1));
		checkAndProcessHome(Stepper::Y, DigitalInController::getDigitalVal(2));
		checkAndProcessHome(Stepper::Z, DigitalInController::getDigitalVal(3));
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
	}
}
