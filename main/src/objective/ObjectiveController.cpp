#pragma once
#include "cJSON.h"
#include "../motor/FocusMotor.h"
#include "../home/HomeMotor.h"
#include "../../cJsonTool.h"
#include "../../JsonKeys.h"
#include "ObjectiveController.h"
#include "Preferences.h"
#include "../laser/LaserController.h"

namespace ObjectiveController
{

	// Store previous laser values for all lasers 0...4
	static int prevLaserVals[5] = {0, 0, 0, 0, 0};

	void beforeObjectiveSwitch()
	{
		// Save current laser values and turn off all lasers
		for (int i = 0; i <= 4; ++i)
		{
			prevLaserVals[i] = LaserController::getLaserVal(i);
			//LaserController::setLaserVal(i, 0); // turn off //TODO: Not really working yet
		}
	}

	void afterObjectiveSwitch()
	{
		// Restore previous laser values
		for (int i = 0; i <= 4; ++i)
		{
			if (prevLaserVals[i] > 0)
			{
				// LaserController::setLaserVal(i, prevLaserVals[i]); //TODO: Not really working yet
			}
		}
	}

	static ObjectiveData data;
	Preferences preferences;
	// Helper to move the Z stepper
	static void moveToPosition(int32_t pos, Stepper axis, int speed, int accel, int qid)
	{
		// Set up the motor data
		uint32_t lowerLimit = -2000; // TODO: should probably not be hardcoded?
		uint32_t upperLimit = 110000;
		if (pos < lowerLimit)
		{
			log_i("Position %i is below lower limit %i, setting to lower limit", pos, lowerLimit);
			pos = lowerLimit;
		}
		else if (pos > upperLimit)
		{
			log_i("Position %i is above upper limit %i, setting to upper limit", pos, upperLimit);
			pos = upperLimit;
		}
		FocusMotor::getData()[axis]->targetPosition = pos;
		FocusMotor::getData()[axis]->speed = speed;
		FocusMotor::getData()[axis]->maxspeed = speed;
		FocusMotor::getData()[axis]->acceleration = accel;
		FocusMotor::getData()[axis]->isEnable = 1;
		FocusMotor::getData()[axis]->isaccelerated = 1;
		FocusMotor::getData()[axis]->isStop = 0;
		FocusMotor::getData()[axis]->stopped = false;
		FocusMotor::getData()[axis]->absolutePosition = 1;
		FocusMotor::getData()[axis]->qid = qid;
		FocusMotor::getData()[axis]->isforever = false; // not forever, just move to position
		FocusMotor::startStepper(axis, 1);
		data.lastTarget = pos;
	}

	int act(cJSON *doc)
	{
		// From Home:  [ 33497][I][HomeMotor.cpp:129] startHome(): Start home for axis 0 with timeout 20000, speed 15000, maxspeed 0, direction -1, endstop polarity 0
		// From here: [ 60522][I][HomeMotor.cpp:129] startHome(): Start home for axis 0 with timeout 10000, speed 20000, maxspeed 20000, direction 1, endstop polarity -1
		/*
				{"task": "/objective_get"}
				{"task":"/objective_act","calibrate":1}
				{"task":"/objective_act","calibrate":1, "homeDirection": 1, "homeEndStopPolarity": -1} calibrate with direction and polarity (e.g. homing to set 0 position, objective positions are aboslute)
				toggle between x1 and x2 objective positions (i.e. slot 1 or slot 2 - toggle has to stay 1)
				{"task":"/objective_act","toggle":1, "speed": 20000, "accel": 400000} 			
				{"task":"/objective_act","move":1,"speed":20000,"accel":20000,"obj":1}			explictely move to slot x1 or x2
				{"task":"/objective_act","x1":  000, "x2": 80000} set explicit positions (in steps )
				{"task":"/objective_act","x1": -1, "x2": 2000}	set current position as x1 and explicit position for x2
				{"task":"/objective_act","z1": 20, "z2": 200}	set current position as z1 and explicit position for z2
				{"task":"/home_act", "home": {"steppers": [{"stepperid":0, "timeout": 20000, "speed": 10000, "direction":-1, "endstoppolarity":1}]}}
				{"task":"/tmc_act", "msteps":16, "rms_current":600, "sgthrs":15, "semin":5, "semax":2, "blank_time":24, "toff":4, "axis":0}
		*/
		preferences.begin("obj", false);
		log_i("objective_Act_fct");
		// print the json
		char *out = cJSON_PrintUnformatted(doc);
		log_i("Objective act %s", out);
		int qid = cJsonTool::getJsonInt(doc, "qid");

		// get default parameters for speed etc.
		int speed = cJsonTool::getJsonInt(doc, "speed", 20000);
		int accel = cJsonTool::getJsonInt(doc, "accel", 20000);
		int obj = cJsonTool::getJsonInt(doc, "obj");

		// Set Positions for X1 and X2 via JSON Input
		// {"task":"/objective_act","x1": 1000, "x2": 2000}
		// if x1 or x2 is -1, set the current position as the objective calibration position
		// {"task":"/objective_act","x1": -1, "x2": 2000}
		if (cJSON_HasObjectItem(doc, "x1"))
		{
			int32_t val = cJsonTool::getJsonInt(doc, "x1");
			if (val == -1)
			{
				log_i("Objective x1 is -1, setting to current position: %i", FocusMotor::getData()[sObjective]->currentPosition);
				data.x1 = FocusMotor::getData()[sObjective]->currentPosition;
				preferences.putInt("x1", data.x1);
			}
			else
			{
				log_i("Objective x1 is %i", val);
				data.x1 = val;
				preferences.putInt("x1", data.x1);
			}
		}
		// Set Positions for X2 via JSON Input
		if (cJSON_HasObjectItem(doc, "x2"))
		{
			int32_t val = cJsonTool::getJsonInt(doc, "x2");
			if (val == -1)
			{
				log_i("Objective x2 is -1, setting to current position: %i", FocusMotor::getData()[sObjective]->currentPosition);
				data.x2 = FocusMotor::getData()[sObjective]->currentPosition;
				preferences.putInt("x2", data.x2);
			}

			else
			{
				log_i("Objective x2 is %i", val);
				data.x2 = val;
				preferences.putInt("x2", data.x2);
			}
		}

		// if we get values for the Z-axis, we want to store this too
		if (cJSON_HasObjectItem(doc, "z1"))
		{
			int32_t val = cJsonTool::getJsonInt(doc, "z1");
			if (val == -1)
			{
				log_i("Objective z1 is -1, setting to current position: %i", FocusMotor::getData()[sObjective]->currentPosition);
				data.z1 = FocusMotor::getData()[sObjective]->currentPosition;
				preferences.putInt("z1", data.z1);
			}
			else
			{
				log_i("Objective z1 is %i", val);
				data.z1 = val;
				preferences.putInt("z1", data.z1);
			}
		}
		if (cJSON_HasObjectItem(doc, "z2"))
		{
			int32_t val = cJsonTool::getJsonInt(doc, "z2");
			if (val == -1)
			{
				log_i("Objective z2 is -1, setting to current position: %i", FocusMotor::getData()[sObjective]->currentPosition);
				data.z2 = FocusMotor::getData()[sObjective]->currentPosition;
				preferences.putInt("z2", data.z2);
			}
			else
			{
				log_i("Objective z2 is %i", val);
				data.z2 = val;
				preferences.putInt("z2", data.z2);
			}
		}
		// Handle move => explictely move to x1 or x2
		if (cJSON_HasObjectItem(doc, "move"))
		{
			beforeObjectiveSwitch();
			log_i("Objective move, to position: %i", obj);
			//{"task":"/objective_act","move":1,"speed":20000,"accel":20000,"obj":1}
			// move to position X1: {"task":"/objective_act","move":1,"speed":20000,"accel":20000,"obj":1}
			// move to position X2: {"task":"/objective_act","move":1,"speed":20000,"accel":20000,"obj":2}

			if (obj == 0)
			{
				// Home the objective (A axis)
				log_i("Objective calibrate");
				HomeMotor::startHome(sObjective, 10000, 20000, 20000, -1, 0, qid, false);
			}
			else if (obj == 1)
			{
				// Move to X1
				log_i("Objective move to x1 at position: %i", data.x1);
				moveToPosition(data.x1, sObjective, speed, accel, qid);
				if (data.z1 != 0)
				{
					log_i("Objective move to z1 at position: %i", data.z1);
					moveToPosition(data.z1, sFocus, speed, accel, qid);
				}
			}
			else if (obj == 2)
			{
				// Move to X2
				log_i("Objective move to x2 at position: %i", data.x2);
				moveToPosition(data.x2, sObjective, speed, accel, qid);
				if (data.z2 != 0)
				{
					log_i("Objective move to z2 at position: %i", data.z2);
					moveToPosition(data.z2, sFocus, speed, accel, qid);
				}
			}
			afterObjectiveSwitch();
			data.currentState = 1;
		}

		// Handle toggle
		if (cJSON_HasObjectItem(doc, "toggle"))
		{
			int toggleVal = cJsonTool::getJsonInt(doc, "toggle");
			if (toggleVal == 1)
			{
				beforeObjectiveSwitch();
				// Decide which position to move to based on the last target
				if (data.currentState == 1)
				{
					moveToPosition(data.x2, sObjective, speed, accel, qid);
					data.currentState = 2;
					if (data.z2 != 0)
					{
						log_i("Objective move to z2 at position: %i", data.z2);
						moveToPosition(data.z2, sFocus, speed, accel, qid);
					}
				}
				else
				{
					moveToPosition(data.x1, sObjective, speed, accel, qid);
					data.currentState = 1;
					if (data.z1 != 0)
					{
						log_i("Objective move to z1 at position: %i", data.z1);
						moveToPosition(data.z1, sFocus, speed, accel, qid);
					}
				}
				afterObjectiveSwitch();
			}
		}

		// Handle calibration (e.g. homing of the axis)
		if (cJSON_HasObjectItem(doc, "calibrate"))
		{
			log_i("Objective calibrate");
			// {"task":"/objective_act","calibrate":1, "homeDirection": -1, "homeEndStopPolarity": 0}

			// read the homing parameters
			int homeTimeout = cJsonTool::getJsonInt(doc, "homeTimeout", 10000);
			int homeMaxspeed = cJsonTool::getJsonInt(doc, "homeMaxspeed", 20000);
			int homeDirection = cJsonTool::getJsonInt(doc, "homeDirection", pinConfig.objectiveHomeDirection);
			int homeEndStopPolarity = cJsonTool::getJsonInt(doc, "homeEndStopPolarity", pinConfig.objectiveHomeEndStopPolarity);
			HomeMotor::startHome(sObjective, homeTimeout, speed, homeMaxspeed, homeDirection, homeEndStopPolarity, qid, false);
			data.currentState = 0;
			data.isHomed = true;
		}

		preferences.putInt("state", data.currentState);
		preferences.end();

		return qid;
	}

	void setIsHomed(bool isHomed)
	{
		// Set the homed status from an external source (e.g. Home Controller)
		data.isHomed = isHomed;
		preferences.begin("obj", false);
		preferences.putBool("isHomed", isHomed);
		preferences.end();
	}

	cJSON *get(cJSON *doc)
	{
		// {"task":"/objective_get"}
		// returns:
		// {"objective":{"x1":1000,"x2":2000,"pos":1000,"isHomed":1,"state":1,"isRunning":0}}
		cJSON *root = cJSON_CreateObject();
		cJSON *obj = cJSON_CreateObject();
		int qid = cJsonTool::getJsonInt(doc, "qid");
		cJSON_AddItemToObject(root, "objective", obj);
		cJsonTool::setJsonInt(obj, "x1", (int)data.x1);
		cJsonTool::setJsonInt(obj, "x2", (int)data.x2);
		cJsonTool::setJsonInt(obj, "z1", (int)data.z1);
		cJsonTool::setJsonInt(obj, "z2", (int)data.z2);
		cJsonTool::setJsonInt(obj, "pos", FocusMotor::getData()[sObjective]->currentPosition);
		cJsonTool::setJsonInt(obj, "isHomed", data.isHomed);
		cJsonTool::setJsonInt(obj, "state", data.currentState);
		cJsonTool::setJsonInt(obj, "isRunning", FocusMotor::isRunning(sObjective));
		cJsonTool::setJsonInt(root, "qid", qid);

		return root;
	}

	void share_changed_event(int pressed)
	{
		log_i("Objective move to x1 at position: %i", data.x1);
		if (pressed)
		{
			// Move to X1
			int speed = 20000;
			int accel = 20000;
			int qid = 0;
			moveToPosition(data.x1, sObjective, speed, accel, qid);
			if (data.z1 != 0)
			{
				log_i("Objective move to z1 at position: %i", data.z1);
				moveToPosition(data.z1, sFocus, speed, accel, qid);
			}
			data.currentState = 1;
		}
	}

	void options_changed_event(int pressed)
	{
		log_i("Objective move to x2 at position: %i", data.x2);
		if (pressed)
		{
			// Move to X2
			int speed = 20000;
			int accel = 20000;
			int qid = 0;
			moveToPosition(data.x2, sObjective, speed, accel, qid);
			if (data.z2 != 0)
			{
				log_i("Objective move to z2 at position: %i", data.z2);
				moveToPosition(data.z2, sFocus, speed, accel, qid);
			}
			data.currentState = 1;
		}
	}

	void setup()
	{
		// Initialize data if needed

		preferences.begin("obj", false);
		data.x1 = preferences.getInt("x1", pinConfig.objectivePositionX1);
		data.x2 = preferences.getInt("x2", pinConfig.objectivePositionX2);
		data.z1 = preferences.getInt("z1", 0);
		data.z2 = preferences.getInt("z2", 0);
		data.isHomed = preferences.getBool("isHomed", false);
		data.lastTarget = preferences.getInt("lastTarg", data.x1);
		preferences.end();

		// TODO: potentially we want to home it? Or rather explictiely do that?
	}

	void loop()
	{
	}


}
