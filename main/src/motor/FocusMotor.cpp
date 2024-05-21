#include <PinConfig.h>
#include "../../config.h"
#include "FocusMotor.h"
#include "../wifi/WifiController.h"
#include "../../cJsonTool.h"
#ifdef USE_TCA9535
#include "../i2c/tca_controller.h"
#endif
#ifdef USE_FASTACCEL
#include "FAccelStep.h"
#endif
#ifdef USE_ACCELSTEP
#include "AccelStep.h"
#endif
#ifdef HOME_DRIVE
#include "HomeDrive.h"
#endif
#ifdef STAGE_SCAN
#include "StageScan.h"
#endif

namespace FocusMotor
{

	MotorData a_dat;
	MotorData x_dat;
	MotorData y_dat;
	MotorData z_dat;
	MotorData *data[4];

	MotorData **getData()
	{
		if (data != nullptr)
			return data;
		else{
			MotorData *mData[4];
			return mData;
		}

	}

#ifdef WIFI
	void sendUpdateToClients(void *p)
	{
		for (;;)
		{
			cJSON *root = cJSON_CreateObject();
			cJSON *stprs = cJSON_CreateArray();
			cJSON_AddItemToObject(root, key_steppers, stprs);
			int added = 0;
			for (int i = 0; i < 4; i++)
			{
				if (!data[i]->stopped)
				{
#ifdef USE_FASTACCEL
					FAccelStep::updateData(i);
#endif
#ifdef USE_ACCELSTEP
					AccelStep::updateData(i);
#endif
					cJSON *item = cJSON_CreateObject();
					cJSON_AddItemToArray(stprs, item);
					cJSON_AddNumberToObject(item, key_stepperid, i);
					cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
					cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);
					added++;
				}
			}
			if (added > 0)
			{
#ifdef WIFI
				WifiController::sendJsonWebSocketMsg(root);
#endif
				// print result - will that work in the case of an xTask?
				Serial.println("++");
				char *s = cJSON_Print(root);
				Serial.println(s);
				free(s);
				Serial.println("--");
			}
			cJSON_Delete(root);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	}
#endif

	void startStepper(int i)
	{
		// log_i("start stepper %i", i);
#ifdef USE_FASTACCEL
		FAccelStep::startFastAccelStepper(i);
#endif
#ifdef USE_ACCELSTEP
		AccelStep::startAccelStepper(i);
#endif
	}

	void parseMotorDriveJson(cJSON *doc)
	{
		#ifdef MOTOR_CONTROLLER
		cJSON *mot = cJSON_GetObjectItemCaseSensitive(doc, key_motor);
		if (mot != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItemCaseSensitive(mot, key_steppers);
			cJSON *stp = NULL;
			if (stprs != NULL)
			{
				cJSON_ArrayForEach(stp, stprs)
				{
					Stepper s = static_cast<Stepper>(cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)));
					data[s]->speed = cJsonTool::getJsonInt(stp, key_speed);
					data[s]->isEnable = cJsonTool::getJsonInt(stp, key_isen);
					data[s]->targetPosition = cJsonTool::getJsonInt(stp, key_position);
					data[s]->isforever = cJsonTool::getJsonInt(stp, key_isforever);
					data[s]->absolutePosition = cJsonTool::getJsonInt(stp, key_isabs);
					data[s]->acceleration = cJsonTool::getJsonInt(stp, key_acceleration);
					data[s]->isaccelerated = cJsonTool::getJsonInt(stp, key_isaccel);
					cJSON *cstop = cJSON_GetObjectItemCaseSensitive(stp, key_isstop);
					if (cstop != NULL)
						stopStepper(s);
					else
						startStepper(s);
					/*log_i("start stepper (act): motor:%i isforver:%i, speed: %i, maxSpeed: %i, target pos: %i, isabsolute: %i, isacceleration: %i, acceleration: %i",
						  s,
						  data[s]->isforever,
						  data[s]->speed,
						  data[s]->maxspeed,
						  data[s]->targetPosition,
						  data[s]->absolutePosition,
						  data[s]->isaccelerated,
						  data[s]->acceleration);*/
				}
			}
			else
				log_i("Motor steppers json is null");
		}
		else
			log_i("Motor json is null");

		#endif
	}

	bool parseSetPosition(cJSON *doc)
	{
		// set position
		cJSON *setpos = cJSON_GetObjectItem(doc, key_setposition);
		// {"task": "/motor_act", "setpos": {"steppers": [{"stepperid": 0, "posval": 100}, {"stepperid": 1, "posval": 0}, {"stepperid": 2, "posval": 0}, {"stepperid": 3, "posval": 0}]}}

		if (setpos != NULL)
		{
			log_d("setpos");
			cJSON *stprs = cJSON_GetObjectItem(setpos, key_steppers);
			if (stprs != NULL)
			{
				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					setPosition(s, cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
					log_i("Setting motor position to %i", cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
				}
			}
			return true; // motor will return pos per socket or serial inside loop. act is fire and forget
		}
		return false;
	}

	void enable(bool en)
	{
		#ifdef USE_FASTACCEL
			FAccelStep::Enable(en);
#endif
#ifdef USE_ACCELSTEP
			AccelStep::Enable(en);
#endif
	}

	void parseEnableMotor(cJSON *doc)
	{
		cJSON *isen = cJSON_GetObjectItemCaseSensitive(doc, key_isen);
		if (isen != NULL)
		{
			enable(isen->valueint);
		}
	}

	void parseAutoEnableMotor(cJSON *doc)
	{
		cJSON *autoen = cJSON_GetObjectItemCaseSensitive(doc, key_isenauto);
#ifdef USE_FASTACCEL
		if (autoen != NULL)
		{
			FAccelStep::setAutoEnable(autoen->valueint);
		}
#endif
	}

#ifdef HOME_DRIVE
	void parseHome(cJSON *doc)
	{
		cJSON *home = cJSON_GetObjectItemCaseSensitive(doc, "home");
		if (home != NULL)
		{
			cJSON *t;
			cJSON_ArrayForEach(t, home)
			{
				log_i("Drive home:%s", t);
				Stepper s = static_cast<Stepper>(t->valueint);
				HomeDrive::driveHome(s);
			}
		}
	}
#endif

#ifdef STAGE_SCAN
	void parseStageScan(cJSON *doc)
	{
		// set trigger
		cJSON *settrigger = cJSON_GetObjectItem(doc, key_settrigger);
		// {"task": "/motor_act", "setTrig": {"steppers": [{"stepperid": 1, "trigPin": 1, "trigOff":0, "trigPer":1}]}}
		// {"task": "/motor_act", "setTrig": {"steppers": [{"stepperid": 2, "trigPin": 2, "trigOff":0, "trigPer":1}]}}
		// {"task":"/motor_act","motor":{"steppers": [{ "stepperid": 1, "position": 5000, "speed": 100000, "isabs": 0, "isaccel":0}]}}
		// {"task":"/motor_act","motor":{"steppers": [{ "stepperid": 2, "position": 5000, "speed": 100000, "isabs": 0, "isaccel":0}]}}
		// {"task": "/motor_get"}
		if (settrigger != NULL)
		{
			log_d("settrigger");
			cJSON *stprs = cJSON_GetObjectItem(settrigger, key_steppers);
			if (stprs != NULL)
			{

				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					FocusMotor::getData()[s]->triggerPin = cJSON_GetObjectItemCaseSensitive(stp, key_triggerpin)->valueint;
					FocusMotor::getData()[s]->offsetTrigger = cJSON_GetObjectItemCaseSensitive(stp, key_triggeroffset)->valueint;
					FocusMotor::getData()[s]->triggerPeriod = cJSON_GetObjectItemCaseSensitive(stp, key_triggerperiod)->valueint;
					log_i("Setting motor trigger offset to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggeroffset)->valueint);
					log_i("Setting motor trigger period to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggerperiod)->valueint);
					log_i("Setting motor trigger pin ID to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggerpin)->valueint);
				}
			}
		}

		// start independent stageScan
		//
		cJSON *stagescan = cJSON_GetObjectItem(doc, "stagescan");
		if (stagescan != NULL)
		{
			StageScan::getStageScanData()->stopped = cJsonTool::getJsonInt(stagescan, "stopped");
			if (StageScan::getStageScanData()->stopped )
			{
				log_i("stagescan stopped");
				return;
			}
			StageScan::getStageScanData()->nStepsLine = cJsonTool::getJsonInt(stagescan, "nStepsLine");
			StageScan::getStageScanData()->dStepsLine = cJsonTool::getJsonInt(stagescan, "dStepsLine");
			StageScan::getStageScanData()->nTriggerLine = cJsonTool::getJsonInt(stagescan, "nTriggerLine");
			StageScan::getStageScanData()->nStepsPixel = cJsonTool::getJsonInt(stagescan, "nStepsPixel");
			StageScan::getStageScanData()->dStepsPixel = cJsonTool::getJsonInt(stagescan, "dStepsPixel");
			StageScan::getStageScanData()->nTriggerPixel = cJsonTool::getJsonInt(stagescan, "nTriggerPixel");
			StageScan::getStageScanData()->delayTimeStep = cJsonTool::getJsonInt(stagescan, "delayTimeStep");
			StageScan::getStageScanData()->nFrames = cJsonTool::getJsonInt(stagescan, "nFrames");
			// xTaskCreate(stageScanThread, "stageScan", pinConfig.STAGESCAN_TASK_STACKSIZE, NULL, 0, &TaskHandle_stagescan_t);
			StageScan::stageScan();
		}
	}
#endif

	int act(cJSON *doc)
	{
		// log_i("motor act");
		int qid = cJsonTool::getJsonInt(doc, "qid");
		// only enable/disable motors
		// {"task":"/motor_act", "isen":1, "isenauto":1}
		//{"task":"/motor_act","home":[1,2]}
		parseEnableMotor(doc);
		// only set motors to autoenable
		parseAutoEnableMotor(doc);
		if (parseSetPosition(doc))
			return 1;
		parseMotorDriveJson(doc);
#ifdef HOME_DRIVE
		parseHome(doc);
#endif
#ifdef STAGE_SCAN
		parseStageScan(doc);
#endif
		return qid;
	}

	// returns json {"motor":{...}} as qid
	cJSON *get(cJSON *docin)
	{
		log_i("get motor");
		cJSON *doc = cJSON_CreateObject();
		cJSON *pos = cJSON_GetObjectItemCaseSensitive(docin, key_position);
		cJSON *stop = cJSON_GetObjectItemCaseSensitive(docin, key_stopped);
		cJSON *mot = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, key_motor, mot);
		cJSON *stprs = cJSON_CreateArray();
		cJSON_AddItemToObject(mot, key_steppers, stprs);

		for (int i = 0; i < 4; i++)
		{
			if (pos != NULL)
			{
				// update position and push it to the json
				data[i]->currentPosition = 1;
				cJSON *aritem = cJSON_CreateObject();
				cJsonTool::setJsonInt(aritem, key_stepperid, i);
#ifdef USE_FASTACCEL
				FAccelStep::updateData(i);
#endif
#ifdef USE_ACCELSTEP
				AccelStep::updateData(i);
#endif

				cJsonTool::setJsonInt(aritem, key_position, data[i]->currentPosition);
				cJSON_AddItemToArray(stprs, aritem);
			}
			else if (stop != NULL)
			{
				cJSON *aritem = cJSON_CreateObject();
				cJsonTool::setJsonInt(aritem, key_stopped, !data[i]->stopped);
				cJSON_AddItemToArray(stprs, aritem);
			}
			else // return the whole config
			{
#ifdef USE_FASTACCEL
				FAccelStep::updateData(i);
#endif
#ifdef USE_ACCELSTEP
				AccelStep::updateData(i);
#endif
				cJSON *aritem = cJSON_CreateObject();
				cJsonTool::setJsonInt(aritem, key_stepperid, i);
				cJsonTool::setJsonInt(aritem, key_position, data[i]->currentPosition);
				cJsonTool::setJsonInt(aritem, "isActivated", data[i]->isActivated);
				cJsonTool::setJsonInt(aritem, key_triggeroffset, data[i]->offsetTrigger);
				cJsonTool::setJsonInt(aritem, key_triggerperiod, data[i]->triggerPeriod);
				cJsonTool::setJsonInt(aritem, key_triggerpin, data[i]->triggerPin);
				cJSON_AddItemToArray(stprs, aritem);
			}
		}
		return doc;
	}

	void setup()
	{
		data[Stepper::A] = &a_dat;
		data[Stepper::X] = &x_dat;
		data[Stepper::Y] = &y_dat;
		data[Stepper::Z] = &z_dat;
		if (data[Stepper::A] == nullptr)
			log_e("Stepper A data NULL");
		if (data[Stepper::X] == nullptr)
			log_e("Stepper X data NULL");
		if (data[Stepper::Y] == nullptr)
			log_e("Stepper Y data NULL");
		if (data[Stepper::Z] == nullptr)
			log_e("Stepper Z data NULL");

		log_i("Setting Up Motor A,X,Y,Z");
	log_i("Setting Up Motor A,X,Y,Z");
	preferences.begin("motor-positions", false);
	if (pinConfig.MOTOR_A_DIR > 0)
	{
		data[Stepper::A]->dirPin = pinConfig.MOTOR_A_DIR;
		data[Stepper::A]->stpPin = pinConfig.MOTOR_A_STEP;
		data[Stepper::A]->currentPosition = preferences.getLong(("motor" + String(Stepper::A)).c_str());
		log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
	}
	if (pinConfig.MOTOR_X_DIR > 0)
	{
		data[Stepper::X]->dirPin = pinConfig.MOTOR_X_DIR;
		data[Stepper::X]->stpPin = pinConfig.MOTOR_X_STEP;
		data[Stepper::X]->currentPosition = preferences.getLong(("motor" + String(Stepper::X)).c_str());
		log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
	}
	if (pinConfig.MOTOR_Y_DIR > 0)
	{
		data[Stepper::Y]->dirPin = pinConfig.MOTOR_Y_DIR;
		data[Stepper::Y]->stpPin = pinConfig.MOTOR_Y_STEP;
		data[Stepper::Y]->currentPosition = preferences.getLong(("motor" + String(Stepper::Y)).c_str());
		log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
	}
	if (pinConfig.MOTOR_Z_DIR > 0)
	{
		data[Stepper::Z]->dirPin = pinConfig.MOTOR_Z_DIR;
		data[Stepper::Z]->stpPin = pinConfig.MOTOR_Z_STEP;
		data[Stepper::Z]->currentPosition = preferences.getLong(("motor" + String(Stepper::Z)).c_str());
		log_i("Motor Z position: %i", data[Stepper::Z]->currentPosition);
	}
	preferences.end();

		// setup trigger pins
		if (pinConfig.DIGITAL_OUT_1 > 0)
			data[Stepper::X]->triggerPin = 1; // pixel^
		if (pinConfig.DIGITAL_OUT_2 > 0)
			data[Stepper::Y]->triggerPin = 2; // line^
		if (pinConfig.DIGITAL_OUT_3 > 0)
			data[Stepper::Z]->triggerPin = 3; // frame^


#ifdef USE_FASTACCEL
#ifdef USE_TCA9535
		if (pinConfig.I2C_SCL > 0)
		{
			FAccelStep::setExternalCallForPin(tca_controller::setExternalPin);
		}
#endif

		FAccelStep::setupFastAccelStepper();
#endif
#ifdef USE_ACCELSTEP
#ifdef USE_TCA9535
		if (pinConfig.I2C_SCL > 0)
		{
			AccelStep::setExternalCallForPin(tca_controller::setExternalPin);
		}
#endif

		AccelStep::setupAccelStepper();
#endif
#ifdef WIFI
		// TODO: This causes the heap to overload?
		//log_i("Creating Task sendUpdateToClients");
		//xTaskCreate(sendUpdateToClients, "sendUpdateToWSClients", pinConfig.MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);
#endif
	}

	void loop()
	{
#ifdef USE_FASTACCEL or USE_ACCELSTEP
		// checks if a stepper is still running
		for (int i = 0; i < 4; i++)
		{
			#ifdef USE_FASTACCEL
			bool isRunning = FAccelStep::isRunning(i);
			#endif
			#ifdef USE_ACCELSTEP
			bool isRunning = AccelStep::isRunning(i);
			#endif
			if (!isRunning && !data[i]->stopped)
			{
				// Only send the information when the motor is halting
				// log_d("Sending motor pos %i", i);
				stopStepper(i);
				sendMotorPos(i, 0);
				preferences.begin("motor-positions", false);
				preferences.putLong(("motor" + String(i)).c_str(), data[i]->currentPosition);
				preferences.end();
			}
		}
#endif
	}

	bool isRunning(int i)
	{
#ifdef USE_FASTACCEL
		return FAccelStep::isRunning(i);
#endif
#ifdef USE_ACCELSTEP
		return AccelStep::isRunning(i);
#endif
	}

	// returns json {"steppers":[...]} as qid
	void sendMotorPos(int i, int arraypos)
	{
#ifdef USE_FASTACCEL
		FAccelStep::updateData(i);
#endif
#ifdef USE_ACCELSTEP
		AccelStep::updateData(i);
#endif
		cJSON *root = cJSON_CreateObject();
		cJSON *stprs = cJSON_CreateArray();
		cJSON_AddItemToObject(root, key_steppers, stprs);
		cJSON *item = cJSON_CreateObject();
		cJSON_AddItemToArray(stprs, item);
		cJSON_AddNumberToObject(item, key_stepperid, i);
		cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
		cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);
		arraypos++;

#ifdef WIFI
		WifiController::sendJsonWebSocketMsg(root);
#endif
		// print result - will that work in the case of an xTask?
		Serial.println("++");
		char *s = cJSON_Print(root);
		Serial.println(s);
		free(s);
		Serial.println("--");
	}

	void stopStepper(int i)
	{
		log_i("Stop Stepper:%i", i);
#ifdef USE_FASTACCEL
		FAccelStep::stopFastAccelStepper(i);
#endif
#ifdef USE_ACCELSTEP
		AccelStep::stopAccelStepper(i);
#endif
	}

	void setPosition(Stepper s, int pos)
	{
#ifdef USE_FASTACCEL
		FAccelStep::setPosition(s, pos);
#endif
	}

	void move(Stepper s, int steps, bool blocking)
	{
#ifdef USE_FASTACCEL
		FAccelStep::move(s, steps, blocking);
#endif
	}
}
