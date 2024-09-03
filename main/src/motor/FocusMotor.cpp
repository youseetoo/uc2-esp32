#include <PinConfig.h>
#include "../../config.h"
#include "FocusMotor.h"
#include "Wire.h"
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
#ifdef HOME_MOTOR
#include "HomeDrive.h"
#endif
#ifdef STAGE_SCAN
#include "StageScan.h"
#endif
#ifdef USE_I2C
#include "../i2c/i2c_controller.h"
#include "WiFi.h"
#endif

namespace FocusMotor
{

	MotorData a_dat;
	MotorData x_dat;
	MotorData y_dat;
	MotorData z_dat;
	MotorData *data[4];

	// for A,X,Y,Z intialize the I2C addresses
	uint8_t i2c_addresses[] = {
		pinConfig.I2C_ADD_MOT_A,
		pinConfig.I2C_ADD_MOT_X,
		pinConfig.I2C_ADD_MOT_Y,
		pinConfig.I2C_ADD_MOT_Z};

	MotorData **getData()
	{
		if (data != nullptr)
			return data;
		else
		{
			MotorData *mData[4];
			return mData;
		}
	}

	void setData(int axis, MotorData *mData)
	{
		getData()[axis] = mData;
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
#elif defined USE_ACCELSTEP
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
		// @KillerInk should this become a build flag? pinConfig.IS_I2C_MASTER?
		log_i("start stepper %i", i);
		if (pinConfig.IS_I2C_MASTER)
		{
			sendMotorDataI2C(*data[i], i); // TODO: This cannot send two motor information simultaenosly
			return;
		}
#ifdef USE_FASTACCEL
		FAccelStep::startFastAccelStepper(i);
#elif defined USE_ACCELSTEP
		AccelStep::startAccelStepper(i);
#endif
	}

	void parseJsonI2C(cJSON *doc)
	{
		/*
		We parse the incoming JSON string to the motor struct and send it via I2C to the correpsonding motor driver
		// TODO: We could reuse the parseMotorDriveJson function and just add the I2C send function?
		*/
		log_i("parseJsonI2C");
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
					data[s]->qid = cJsonTool::getJsonInt(doc, "qid");
					data[s]->speed = cJsonTool::getJsonInt(stp, key_speed);
					data[s]->isEnable = cJsonTool::getJsonInt(stp, key_isen);
					data[s]->targetPosition = cJsonTool::getJsonInt(stp, key_position);
					data[s]->isforever = cJsonTool::getJsonInt(stp, key_isforever);
					data[s]->absolutePosition = cJsonTool::getJsonInt(stp, key_isabs);
					data[s]->acceleration = cJsonTool::getJsonInt(stp, key_acceleration);
					data[s]->isaccelerated = cJsonTool::getJsonInt(stp, key_isaccel);
					cJSON *cstop = cJSON_GetObjectItemCaseSensitive(stp, key_isstop);
					if (cstop != NULL)
					{
						data[s]->stopped = true;
						stopStepper(s);
					}
					else
					{
						data[s]->stopped = false;
						startStepper(s);
					}
				}
			}
			else
			{
				log_i("Motor steppers json is null");
			}
		}
		else
		{
			log_i("Motor json is null");
		}
	}

	void parseMotorDriveJson(cJSON *doc)
	{
		/*
		We receive a JSON string e.g. in the form:
		{"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": -10000, "speed": 20000, "isabs": 0.0, "isaccel": 1, "accel":10000, "isen": true}]}, "qid": 5}
		And assign it to the different motors by sending the converted MotorData to the corresponding motor driver via I2C
		*/
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
					log_i("start stepper from parseMotorDriveJson");
					Stepper s = static_cast<Stepper>(cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)));
					data[s]->qid = cJsonTool::getJsonInt(doc, "qid");
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

		// set dual axis if necessary
		cJSON *dualaxisZ = cJSON_GetObjectItemCaseSensitive(doc, key_home_isDualAxis);
		if (dualaxisZ != NULL)
		{
			// {"task": "/motor_act", "isDualAxisZ": 1}
			// store this in the preferences
			Preferences preferences;
			isDualAxisZ = dualaxisZ->valueint;
			const char *prefNamespace = "UC2";
			preferences.begin(prefNamespace, false);
			preferences.putBool("dualAxZ", isDualAxisZ);
			log_i("isDualAxisZ is set to: %i", isDualAxisZ);
			preferences.end();
		}
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
#elif defined USE_ACCELSTEP
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

#ifdef HOME_MOTOR
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
			if (StageScan::getStageScanData()->stopped)
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

	// returns json {"motor":{...}} as qid
	cJSON *get(cJSON *docin)
	{
		// {"task": "/motor_get", "qid": 1}
		log_i("get motor");
		cJSON *doc = cJSON_CreateObject();
		int qid = cJsonTool::getJsonInt(docin, "qid");
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
#elif defined USE_ACCELSTEP
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
#elif defined USE_ACCELSTEP
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
		cJSON_AddItemToObject(doc, "qid", cJSON_CreateNumber(qid));
		return doc;
	}

	int act(cJSON *doc)
	{
		log_i("motor act");
		int qid = cJsonTool::getJsonInt(doc, "qid");

		// parse json to motor struct and send over I2C
		if (pinConfig.IS_I2C_MASTER)
		{
			parseJsonI2C(doc);
			return qid;
		}

		// only enable/disable motors
		// {"task":"/motor_act", "isen":1, "isenauto":1}
		parseEnableMotor(doc);

		// only set motors to autoenable
		// {"task":"/motor_act", "isen":1, "isenauto":1, "qid":1}
		parseAutoEnableMotor(doc);

		// set position of motors in eeprom
		// {"task": "/motor_act", "setpos": {"steppers": [{"stepperid": 0, "posval": 1000}]}, "qid": 37}
		parseSetPosition(doc);

		// move motor drive
		// {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": -10000, "speed": 20000, "isabs": 0.0, "isaccel": 1, "accel":20000, "isen": true}]}, "qid": 5}
		parseMotorDriveJson(doc);
#ifdef HOME_MOTOR
		parseHome(doc);
#endif
#ifdef STAGE_SCAN
		parseStageScan(doc);
#endif
		return qid;
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

		// Read dual axis from preferences if available
		Preferences preferences;
		const char *prefNamespace = "UC2";
		preferences.begin(prefNamespace, false);
		isDualAxisZ = preferences.getBool("dualAxZ", pinConfig.isDualAxisZ);
		preferences.end();

		// setup motor pins
		log_i("Setting Up Motor A,X,Y,Z");
#ifdef USE_FASTACCEL or USE_ACCELSTEP
		preferences.begin("motpos", false);
		if (pinConfig.MOTOR_A_STEP >= 0)
		{
			data[Stepper::A]->dirPin = pinConfig.MOTOR_A_DIR;
			data[Stepper::A]->stpPin = pinConfig.MOTOR_A_STEP;
			data[Stepper::A]->currentPosition = preferences.getLong(("motor" + String(Stepper::A)).c_str());
			log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
		}
		if (pinConfig.MOTOR_X_STEP >= 0)
		{
			data[Stepper::X]->dirPin = pinConfig.MOTOR_X_DIR;
			data[Stepper::X]->stpPin = pinConfig.MOTOR_X_STEP;
			data[Stepper::X]->currentPosition = preferences.getLong(("motor" + String(Stepper::X)).c_str());
			log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
		}
		if (pinConfig.MOTOR_Y_STEP >= 0)
		{
			data[Stepper::Y]->dirPin = pinConfig.MOTOR_Y_DIR;
			data[Stepper::Y]->stpPin = pinConfig.MOTOR_Y_STEP;
			data[Stepper::Y]->currentPosition = preferences.getLong(("motor" + String(Stepper::Y)).c_str());
			log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
		}
		if (pinConfig.MOTOR_Z_STEP >= 0)
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

#endif

#ifdef USE_FASTACCEL
#ifdef USE_TCA9535
		if (pinConfig.I2C_SCL >= 0)
		{
			FAccelStep::setExternalCallForPin(tca_controller::setExternalPin);
		}
#endif
		FAccelStep::setupFastAccelStepper();
#elif defined USE_ACCELSTEP
#ifdef USE_TCA9535
		if (pinConfig.I2C_SCL >= 0)
		{
			AccelStep::setExternalCallForPin(tca_controller::setExternalPin);
		}
#endif
		AccelStep::setupAccelStepper();
#endif

#ifdef WIFI
		// TODO: This causes the heap to overload?
		// log_i("Creating Task sendUpdateToClients");
		// xTaskCreate(sendUpdateToClients, "sendUpdateToWSClients", pinConfig.MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);
#endif
	}

	void loop()
	{
		// checks if a stepper is still running
		for (int i = 0; i < 4; i++)
		{
			bool isRunning = false;
#ifdef USE_FASTACCEL
			isRunning = FAccelStep::isRunning(i);
#elif defined USE_ACCELSTEP
			isRunning = AccelStep::isRunning(i);
#endif
			// if motor is connected via I2C, we have to pull the data from the slave's register
			if (pinConfig.IS_I2C_MASTER)
			{
				pullMotorDataI2CTick[i]++;
				if (pullMotorDataI2CTick[i] > 10)
				{
					log_d("Request Motor State from Motor %i", i);
					MotorState mMotorState = pullMotorDataI2C(i);
					isRunning = mMotorState.isRunning;
					pullMotorDataI2CTick[i] = 0;
					return;
					// TODO check if motor is still running and if not, report position to serial
				}
			}

			if (!isRunning && !data[i]->stopped && !pinConfig.IS_I2C_MASTER)
			{
				// TODO: REadout register on slave side and check if destination
				// Only send the information when the motor is halting
				// log_d("Sending motor pos %i", i);
				stopStepper(i);
				sendMotorPos(i, 0);
				preferences.begin("motpos", false);
				preferences.putLong(("motor" + String(i)).c_str(), data[i]->currentPosition);
				preferences.end();
			}
			if (0) Serial.println("Motor "+String(i)+"is running: " + String(isRunning));

		}
	}

	MotorState pullMotorDataI2C(int axis)
	{
		// we pull the data from the slave's register
		uint8_t slave_addr = axis2address(axis);

		// Request data from the slave but only if inside i2cAddresses
		if (!i2c_controller::isAddressInI2CDevices(slave_addr))
		{
			return MotorState();
		}
		Wire.requestFrom(slave_addr, sizeof(MotorState));
		MotorState motorState; // Initialize with default values
		// Check if the expected amount of data is received
		if (Wire.available() == sizeof(MotorState))
		{
			Wire.readBytes((uint8_t *)&motorState, sizeof(motorState));
		}
		else
		{
			log_e("Error: Incorrect data size received");
		}

		return motorState;
	}

	bool isRunning(int i)
	{
#ifdef USE_FASTACCEL
		return FAccelStep::isRunning(i);
#elif defined USE_ACCELSTEP
		return AccelStep::isRunning(i);
#endif
	}

	// returns json {"steppers":[...]} as qid
	void sendMotorPos(int i, int arraypos)
	{
#ifdef USE_FASTACCEL
		FAccelStep::updateData(i);
#elif defined USE_ACCELSTEP
		AccelStep::updateData(i);
#endif
		cJSON *root = cJSON_CreateObject();
		if (root == NULL)
			return; // Handle allocation failure

		cJSON *stprs = cJSON_CreateArray();
		if (stprs == NULL)
		{
			cJSON_Delete(root);
			return; // Handle allocation failure
		}
		cJSON_AddItemToObject(root, key_steppers, stprs);
		cJSON_AddNumberToObject(root, "qid", data[i]->qid);

		cJSON *item = cJSON_CreateObject();
		if (item == NULL)
		{
			cJSON_Delete(root);
			return; // Handle allocation failure
		}
		cJSON_AddItemToArray(stprs, item);
		cJSON_AddNumberToObject(item, key_stepperid, i);
		cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
		cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);
		arraypos++;

#ifdef WIFI
		WifiController::sendJsonWebSocketMsg(root);
#endif

		// Print result - will that work in the case of an xTask?
		Serial.println("++");
		char *s = cJSON_Print(root);
		if (s != NULL)
		{
			Serial.println(s);
			free(s);
		}
		Serial.println("--");

		cJSON_Delete(root); // Free the root object, which also frees all nested objects
	}

	void stopStepper(int i)
	{
		log_i("Stop Stepper:%i", i);
#ifdef USE_FASTACCEL
		FAccelStep::stopFastAccelStepper(i);
#elif defined USE_ACCELSTEP
		AccelStep::stopAccelStepper(i);
#endif
		if (pinConfig.IS_I2C_MASTER)
		{
			sendMotorDataI2C(*data[i], i); // TODO: This cannot send two motor information simultaenosly
		}
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

	void sendMotorDataI2C(MotorData motorData, uint8_t axis)
	{
		uint8_t slave_addr = axis2address(axis);
		log_i("MotorData to axis: %i", axis);

		// TODO: should we have this inside the I2C controller?
		Wire.beginTransmission(slave_addr);

		// Cast the structure to a byte array
		uint8_t *dataPtr = (uint8_t *)&motorData;
		int dataSize = sizeof(MotorData);

		// Send the byte array over I2C
		for (int i = 0; i < dataSize; i++)
		{
			Wire.write(dataPtr[i]);
		}
		int err = Wire.endTransmission();
		if (err != 0)
		{
			log_e("Error sending motor data to I2C slave at address %i", slave_addr);
		}
		else
		{
			log_i("Motor data sent to I2C slave at address %i", slave_addr);
		}
	}

	int axis2address(int axis)
	{
		if (axis >= 0 && axis < 4)
		{
			return i2c_addresses[axis];
		}
		return 0;
	}
}
