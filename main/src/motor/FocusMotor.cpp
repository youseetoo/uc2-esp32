#include <PinConfig.h>
#ifdef FOCUS_MOTOR
#include "../../config.h"
#include "FocusMotor.h"
#include "../wifi/WifiController.h"
#include "../../cJsonTool.h"
#ifdef USE_TCA9535
#include "../i2c/tca9535.h"
#endif
#ifdef USE_FASTACCEL
#include "FAccelStep.h"
#endif
#ifdef USE_ACCELSTEP
#include "AccelStep.h"
#endif

namespace FocusMotor
{

	MotorData a_dat;
	MotorData x_dat;
	MotorData y_dat;
	MotorData z_dat;
	MotorData * data[4];

	MotorData ** getData()
	{
		return data;
	}
	

	void sendUpdateToClients(void *p)
	{
		for (;;)
		{
			int arraypos = 0;
			for (int i = 0; i < 4; i++)
			{
				if (!data[i]->stopped)
				{
					sendMotorPos(i, arraypos);
				}
			}
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	}

	bool externalPinCallback(uint8_t pin, uint8_t value)
	{
		return setExternalPin(pin, value);
	}

	void startStepper(int i)
	{
		//log_i("start stepper %i", i);
#ifdef USE_FASTACCEL
		FAccelStep::startFastAccelStepper(i);
#endif
#ifdef USE_ACCELSTEP
		AccelStep::startAccelStepper(i);
#endif
	}

	int act(cJSON *doc)
	{
		//log_i("motor act");

		// only enable/disable motors
		// {"task":"/motor_act", "isen":1, "isenauto":1}
		cJSON *isen = cJSON_GetObjectItemCaseSensitive(doc, key_isen);
		if (isen != NULL)
		{
#ifdef USE_FASTACCEL
			FAccelStep::Enable(isen->valueint);
#endif
#ifdef USE_ACCELSTEP
			AccelStep::Enable(isen->valueint);
#endif
		}

		// only set motors to autoenable
		cJSON *autoen = cJSON_GetObjectItemCaseSensitive(doc, key_isenauto);
#ifdef USE_FASTACCEL
		if (autoen != NULL)
		{
			FAccelStep::setAutoEnable(autoen->valueint);
		}
#endif
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
			return 1; // motor will return pos per socket or serial inside loop. act is fire and forget
		}

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
		return 1;
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
				cJSON_AddItemToArray(stprs, aritem);
			}
		}
		return doc;
	}

	bool setExternalPin(uint8_t pin, uint8_t value)
	{
		// This example returns the previous value of the output.
		// Consequently, FastAccelStepper needs to call setExternalPin twice
		// in order to successfully change the output value.
		pin = pin & ~PIN_EXTERNAL_FLAG;
#ifdef USE_TCA9535
		if (pin == 100) // enable
			outRegister.Port.P0.bit.Bit0 = value;
		if (pin == 101) // x
			outRegister.Port.P0.bit.Bit1 = value;
		if (pin == 102) // y
			outRegister.Port.P0.bit.Bit2 = value;
		if (pin == 103) // z
			outRegister.Port.P0.bit.Bit3 = value;
		if (pin == 104) // a
			outRegister.Port.P0.bit.Bit4 = value;
		log_i("external pin cb for pin:%d value:%d", pin, value);
		if (ESP_OK != TCA9535WriteOutput(&outRegister))
			log_e("i2c write failed");
#endif
		return value;
	}

#ifdef USE_TCA9535
	void dumpRegister(const char *name, TCA9535_Register configRegister)
	{
		log_i("%s port0 b0:%i, b1:%i, b2:%i, b3:%i, b4:%i, b5:%i, b6:%i, b7:%i",
			  name,
			  configRegister.Port.P0.bit.Bit0,
			  configRegister.Port.P0.bit.Bit1,
			  configRegister.Port.P0.bit.Bit2,
			  configRegister.Port.P0.bit.Bit3,
			  configRegister.Port.P0.bit.Bit4,
			  configRegister.Port.P0.bit.Bit5,
			  configRegister.Port.P0.bit.Bit6,
			  configRegister.Port.P0.bit.Bit7);
		/*log_i("%s port1 b0:%i, b1:%i, b2:%i, b3:%i, b4:%i, b5:%i, b6:%i, b7:%i",
			  name,
			  configRegister.Port.P1.bit.Bit0,
			  configRegister.Port.P1.bit.Bit1,
			  configRegister.Port.P1.bit.Bit2,
			  configRegister.Port.P1.bit.Bit3,
			  configRegister.Port.P1.bit.Bit4,
			  configRegister.Port.P1.bit.Bit5,
			  configRegister.Port.P1.bit.Bit6,
			  configRegister.Port.P1.bit.Bit7);*/
	}

	void init_tca()
	{
		if (ESP_OK != TCA9535Init(pinConfig.I2C_SCL, pinConfig.I2C_SDA, pinConfig.I2C_ADD))
			log_e("failed to init tca9535");
		else
			log_i("tca9535 init!");
		TCA9535_Register configRegister;
		TCA9535ReadInput(&configRegister);
		dumpRegister("Input", configRegister);
		TCA9535ReadOutput(&configRegister);
		dumpRegister("Output", configRegister);
		TCA9535ReadPolarity(&configRegister);
		dumpRegister("Polarity", configRegister);
		TCA9535ReadConfig(&configRegister);
		dumpRegister("Config", configRegister);

		configRegister.Port.P0.bit.Bit0 = 0; // motor enable
		configRegister.Port.P0.bit.Bit1 = 0; // x
		configRegister.Port.P0.bit.Bit2 = 0; // y
		configRegister.Port.P0.bit.Bit3 = 0; // z
		configRegister.Port.P0.bit.Bit4 = 0; // a
		configRegister.Port.P0.bit.Bit5 = 1;
		configRegister.Port.P0.bit.Bit6 = 1;
		configRegister.Port.P0.bit.Bit7 = 1;

		/*configRegister.Port.P1.bit.Bit0 = 1; // motor enable
		configRegister.Port.P1.bit.Bit1 = 1; // x
		configRegister.Port.P1.bit.Bit2 = 1; // y
		configRegister.Port.P1.bit.Bit3 = 1; // z
		configRegister.Port.P1.bit.Bit4 = 1; // a
		configRegister.Port.P1.bit.Bit5 = 0;
		configRegister.Port.P1.bit.Bit6 = 0;
		configRegister.Port.P1.bit.Bit7 = 0;*/
		if (ESP_OK != TCA9535WriteConfig(&configRegister))
			log_e("failed to write config to tca9535");
		else
			log_i("tca9535 config written!");

		TCA9535ReadConfig(&configRegister);
		dumpRegister("Config", configRegister);
	}
#endif

	void setup()
	{
		data[Stepper::A] = &a_dat;
		data[Stepper::X] = &x_dat;
		data[Stepper::Y] = &y_dat;
		data[Stepper::Z] = &z_dat;
		if(data[Stepper::A] == nullptr)
			log_e("Stepper A data NULL");
		if(data[Stepper::X] == nullptr)
			log_e("Stepper X data NULL");
		if(data[Stepper::Y] == nullptr)
			log_e("Stepper Y data NULL");
		if(data[Stepper::Z] == nullptr)
			log_e("Stepper Z data NULL");
		
		log_i("Setting Up Motor A,X,Y,Z");
		preferences.begin("motor-positions", false);
		if (pinConfig.MOTOR_A_DIR > 0)
			data[Stepper::A]->currentPosition = preferences.getLong(("motor" + String(Stepper::A)).c_str());
		if (pinConfig.MOTOR_X_DIR > 0)
			data[Stepper::X]->currentPosition = preferences.getLong(("motor" + String(Stepper::X)).c_str());
		if (pinConfig.MOTOR_Y_DIR > 0)
			data[Stepper::Y]->currentPosition = preferences.getLong(("motor" + String(Stepper::Y)).c_str());
		if (pinConfig.MOTOR_Z_DIR > 0)
			data[Stepper::Z]->currentPosition = preferences.getLong(("motor" + String(Stepper::Z)).c_str());
		preferences.end();
#ifdef USE_FASTACCEL
#ifdef USE_TCA9535
		if (pinConfig.I2C_SCL > 0)
		{
			init_tca();
			FAccelStep::setExternalCallForPin(externalPinCallback);
		}
#endif

		FAccelStep::setupFastAccelStepper();
#endif
#ifdef USE_ACCELSTEP
#ifdef USE_TCA9535
		if (pinConfig.I2C_SCL > 0)
		{
			init_tca();
			AccelStep::setExternalCallForPin(externalPinCallback);
		}
#endif

		AccelStep::setupAccelStepper();
#endif
		log_i("Creating Task sendUpdateToClients");
		// xTaskCreate(sendUpdateToClients, "sendUpdateToClients", 4096, NULL, 6, NULL);
	}

	// dont use it, it get no longer triggered from modulehandler
	void loop()
	{
#ifdef USE_FASTACCEL
		// checks if a stepper is still running
		for (int i = 0; i < 4; i++)
		{
			bool isRunning = FAccelStep::isRunning(i);
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

	void disableEnablePin(int i)
	{
		/*
		if (data[Stepper::A]->stopped &&
			data[Stepper::X]->stopped &&
			data[Stepper::Y]->stopped &&
			data[Stepper::Z]->stopped &&
			power_enable)
		{
			pinMode(pinConfig.MOTOR_ENABLE, OUTPUT);
			digitalWrite(pinConfig.MOTOR_ENABLE, LOW ^ pinConfig.MOTOR_ENABLE_INVERTED);
			power_enable = false;
		}
		*/
	}

	void enableEnablePin(int i)
	{
		/*
		if (!power_enable)
		{
			pinMode(pinConfig.MOTOR_ENABLE, OUTPUT);
			digitalWrite(pinConfig.MOTOR_ENABLE, HIGH ^ pinConfig.MOTOR_ENABLE_INVERTED);
			power_enable = true;
		}
		*/
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
#endif
