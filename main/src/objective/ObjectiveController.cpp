#pragma once
#include "cJSON.h"
#include "../motor/FocusMotor.h"
#include "../home/HomeMotor.h"
#include "../../cJsonTool.h"
#include "../../JsonKeys.h"
#include "ObjectiveController.h"
#include "Preferences.h"

namespace ObjectiveController
{

	static ObjectiveData data;
	Preferences preferences;
	// Helper to move the Z stepper
	static void moveToPosition(long pos, int speed, int accel)
	{
		// Set up the motor data
		FocusMotor::getData()[sObjective]->targetPosition = pos;
		FocusMotor::getData()[sObjective]->speed = speed;
		FocusMotor::getData()[sObjective]->maxspeed = speed;
		FocusMotor::getData()[sObjective]->acceleration = accel;
		FocusMotor::getData()[sObjective]->isEnable = 1;
		FocusMotor::getData()[sObjective]->isaccelerated = 1;
		FocusMotor::getData()[sObjective]->isStop = 0;
		FocusMotor::getData()[sObjective]->stopped = false;
		FocusMotor::getData()[sObjective]->absolutePosition = 1;
		FocusMotor::startStepper(sObjective, true);
		data.lastTarget = pos;
	}

	int act(cJSON *doc)
	{
		// {"task":"/objective_act", "x1": 1}
		// {"task":"/objective_act","calibrate":1}
		// {"task":"/objective_act","calibrate":1, "homeDirection": 1, "homeEndStopPolarity": -1}
		// From Home:  [ 33497][I][HomeMotor.cpp:129] startHome(): Start home for axis 0 with timeout 20000, speed 15000, maxspeed 0, direction -1, endstop polarity 0
		// From here: [ 60522][I][HomeMotor.cpp:129] startHome(): Start home for axis 0 with timeout 10000, speed 20000, maxspeed 20000, direction 1, endstop polarity -1
		// {"task":"/objective_act","toggle":1, "speed": 20000, "accel": 20000}
		// {"task":"/objective_act","move":1,"speed":20000,"accel":20000,"obj":1}
		preferences.begin("obj", false);
		log_i("objective_Act_fct");
		// print the json
		char *out = cJSON_PrintUnformatted(doc);
		log_i("Objective act %s", out);
		int qid = cJsonTool::getJsonInt(doc, "qid");

		// Handle x1/x2 setting
		if (cJSON_HasObjectItem(doc, "x1"))
		{
			long val = cJsonTool::getJsonInt(doc, "x1");
			if (val == -1)
			{
				data.x1 = FocusMotor::getData()[sObjective]->currentPosition;
			}
			else
			{
				data.x1 = val;
			}
		}
		if (cJSON_HasObjectItem(doc, "x2"))
		{
			long val = cJsonTool::getJsonInt(doc, "x2");
			if (val == -1)
			{
				data.x2 = FocusMotor::getData()[sObjective]->currentPosition;
			}

			else
			{
				data.x2 = val;
			}
		}

		// Handle move
		if (cJSON_HasObjectItem(doc, "move"))
		{
			//{"task":"/objective_act","move":1,"speed":20000,"accel":20000,"obj":1}
			int speed = cJsonTool::getJsonInt(doc, "speed", 20000);
			int accel = cJsonTool::getJsonInt(doc, "accel", 20000);
			int obj  = cJsonTool::getJsonInt(doc, "obj");
			if (obj == 0)
			{
				// Home Z
				HomeMotor::startHome(sObjective, 10000, 20000, 20000, -1, 0, qid, false);
			}
			else if (obj == 1)
			{
				moveToPosition(data.x1, speed, accel);
			}
			else if (obj == 2)
			{
				moveToPosition(data.x2, speed, accel);
			}
		}


		// Handle toggle
		if (cJSON_HasObjectItem(doc, "toggle"))
		{
			int toggleVal = cJsonTool::getJsonInt(doc, "toggle");
			if (toggleVal == 1)
			{
				int speed = cJsonTool::getJsonInt(doc, "speed", 20000);
				int accel = cJsonTool::getJsonInt(doc, "accel", 20000);

				// Decide which position to move to based on the last target
				if (data.lastTarget == data.x1){
					moveToPosition(data.x2, speed, accel);
					data.currentState = 2;
				}
				else{
					moveToPosition(data.x1, speed, accel);
					data.currentState = 1;
				}
			}
		}

		// Handle calibration (e.g. homing of the axis)
		if (cJSON_HasObjectItem(doc, "calibrate"))
		{
			log_i("Objective calibrate");
			// {"task":"/objective_act","calibrate":1, "homeDirection": -1, "homeEndStopPolarity": 0}

			// read the homing parameters
			int homeTimeout = cJsonTool::getJsonInt(doc, "homeTimeout", 10000);
			int homeSpeed = cJsonTool::getJsonInt(doc, "homeSpeed", 20000);
			int homeMaxspeed = cJsonTool::getJsonInt(doc, "homeMaxspeed", 20000);
			int homeDirection = cJsonTool::getJsonInt(doc, "homeDirection", pinConfig.objectiveHomeDirection);
			int homeEndStopPolarity = cJsonTool::getJsonInt(doc, "homeEndStopPolarity", pinConfig.objectiveHomeEndStopPolarity);
			HomeMotor::startHome(sObjective, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, qid, false);
			data.isHomed = true;
		}
		preferences.end();

		return qid;
	}

	cJSON *get(cJSON *doc)
	{
		cJSON *root = cJSON_CreateObject();
		cJSON *obj = cJSON_CreateObject();
		cJSON_AddItemToObject(root, "objective", obj);

		cJsonTool::setJsonInt(obj, "x1", (int)data.x1);
		cJsonTool::setJsonInt(obj, "x2", (int)data.x2);
		cJsonTool::setJsonInt(obj, "currentPosition", FocusMotor::getData()[sObjective]->currentPosition);
		cJsonTool::setJsonInt(obj, "isHomed", data.isHomed);

		return root;
	}

	void setup()
	{
		// Initialize data if needed

		preferences.begin("obj", false);
		data.x1 = preferences.getInt("x1", pinConfig.objectivePositionX1);
		data.x2 = preferences.getInt("x2", pinConfig.objectivePositionX2);
		data.isHomed = preferences.getBool("isHomed", false);
		data.lastTarget = preferences.getInt("lastTarg", data.x1);
		preferences.end();

		// TODO: potentially we want to home it?
	}

	void loop()
	{
		// Check if the Z axis is homed by querying home status
		// If homing is done, set data.isHomed = true
		// or read from existing home logic if needed
		// For example, we can check HomeMotor state for Stepper::Z
		if (!data.isHomed)
		{
			// If the homing routine has completed
			if (!HomeMotor::getHomeData()[sObjective]->homeIsActive)
			{
				data.isHomed = true;
			}
		}
	}
}
