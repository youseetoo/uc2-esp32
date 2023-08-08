#include "../../config.h"
#include "FocusMotor.h"
#include "../wifi/WifiController.h"

void sendUpdateToClients(void *p)
{
	for (;;)
	{
		int arraypos = 0;
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		for (int i = 0; i < motor->data.size(); i++)
		{
			if (!motor->data[i]->stopped)
			{
				motor->sendMotorPos(i, arraypos);
			}
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

bool externalPinCallback(uint8_t pin, uint8_t value)
{
	FocusMotor *m = (FocusMotor *)moduleController.get(AvailableModules::motor);
	return m->setExternalPin(pin, value);
}

FocusMotor::FocusMotor() : Module() { log_i("ctor"); }
FocusMotor::~FocusMotor() { log_i("~ctor"); }

int FocusMotor::act(cJSON *doc)
{
	log_i("motor act");

	// only enable/disable motors
	// {"task":"/motor_act", "isen":1, "isenauto":1}
	cJSON *isen = cJSON_GetObjectItemCaseSensitive(doc, key_isen);
	if (isen != NULL)
	{
		if (pinConfig.useFastAccelStepper)
		{
			faccel.Enable(isen->valueint);
		}
		else
		{
			accel.Enable(isen->valueint);
		}
	}

	cJSON *autoen = cJSON_GetObjectItemCaseSensitive(doc, key_isenauto);
	if (autoen != NULL)
	{
		if (pinConfig.useFastAccelStepper)
		{
			faccel.setAutoEnable(autoen->valueint);
		}
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
				data[s]->speed = getJsonInt(stp, key_speed);
				data[s]->isEnable = getJsonInt(stp, key_isen);
				data[s]->targetPosition = getJsonInt(stp, key_position);
				data[s]->isforever = getJsonInt(stp, key_isforever);
				data[s]->absolutePosition = getJsonInt(stp, key_isabs);
				data[s]->acceleration = getJsonInt(stp, key_acceleration);
				data[s]->isaccelerated = getJsonInt(stp, key_isaccel);
				cJSON *cstop = cJSON_GetObjectItemCaseSensitive(stp, key_isstop);
				if (cstop != NULL || (!data[s]->isforever && !data[s]->absolutePosition && !data[s]->stopped))
					stopStepper(s);
				else
					startStepper(s);
				log_i("start stepper (act): motor:%i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i, isacceleration: %i, acceleration: %i", s, data[s]->isforever, data[s]->speed, data[s]->maxspeed, data[s]->targetPosition, data[s]->absolutePosition, data[s]->isaccelerated, data[s]->acceleration);
			}
		}
		else
			log_i("Motor steppers json is null");
	}
	else
		log_i("Motor json is null");
	return 1;
}

void FocusMotor::startStepper(int i)
{
	if (pinConfig.useFastAccelStepper)
		faccel.startFastAccelStepper(i);
	else
		accel.startAccelStepper(i);
}

cJSON *FocusMotor::get(cJSON *docin)
{
	log_i("get motor");
	cJSON *doc = cJSON_CreateObject();
	cJSON *pos = cJSON_GetObjectItemCaseSensitive(docin, key_position);
	cJSON *mot = cJSON_CreateObject();
	cJSON_AddItemToObject(doc, key_motor, mot);
	cJSON *stprs = cJSON_CreateArray();
	cJSON_AddItemToObject(mot, key_steppers, stprs);
	if (pos != NULL)
	{
		for (int i = 0; i < data.size(); i++)
		{
			// update position and push it to the json
			data[i]->currentPosition = 1;
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stepperid, i);
			if (pinConfig.useFastAccelStepper)
				faccel.updateData(i);
			else
				accel.updateData(i);

			setJsonInt(aritem, key_position, data[i]->currentPosition);
			cJSON_AddItemToArray(stprs, aritem);
		}
		return doc;
	}

	cJSON *stop = cJSON_GetObjectItemCaseSensitive(docin, key_stopped);
	if (stop != NULL)
	{
		for (int i = 0; i < data.size(); i++)
		{
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stopped, !data[i]->stopped);
			cJSON_AddItemToArray(stprs, aritem);
		}
		return doc;
	}

	// return the whole config
	for (int i = 0; i < data.size(); i++)
	{
		if (pinConfig.useFastAccelStepper)
			faccel.updateData(i);
		else
			accel.updateData(i);
		cJSON *aritem = cJSON_CreateObject();
		setJsonInt(aritem, key_stepperid, i);
		setJsonInt(aritem, key_position, data[i]->currentPosition);
		cJSON_AddItemToArray(stprs, aritem);
	}
	return doc;
}

bool FocusMotor::setExternalPin(uint8_t pin, uint8_t value)
{
	// This example returns the previous value of the output.
	// Consequently, FastAccelStepper needs to call setExternalPin twice
	// in order to successfully change the output value.
	pin = pin & ~PIN_EXTERNAL_FLAG;
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
	//log_i("external pin cb for pin:%d value:%d", pin, value);
	if (ESP_OK != _tca9535->TCA9535WriteOutput(&outRegister))
		// if (ESP_OK != _tca9535->TCA9535WriteSingleRegister(TCA9535_INPUT_REG0,outRegister.Port.P0.asInt))
		log_e("i2c write failed");

	return value;
}

void FocusMotor::dumpRegister(const char *name, TCA9535_Register configRegister)
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

void FocusMotor::init_tca()
{
	gpio_pad_select_gpio(pinConfig.I2C_INT);
	gpio_set_direction(pinConfig.I2C_INT, GPIO_MODE_INPUT);
	gpio_set_pull_mode(pinConfig.I2C_INT, GPIO_PULLUP_ONLY);
	_tca9535 = new tca9535();

	if (ESP_OK != _tca9535->TCA9535Init(pinConfig.I2C_SCL, pinConfig.I2C_SDA, pinConfig.I2C_ADD))
		log_e("failed to init tca9535");
	else
		log_i("tca9535 init!");
	TCA9535_Register configRegister;
	_tca9535->TCA9535ReadInput(&configRegister);
	dumpRegister("Input", configRegister);
	_tca9535->TCA9535ReadOutput(&configRegister);
	dumpRegister("Output", configRegister);
	_tca9535->TCA9535ReadPolarity(&configRegister);
	dumpRegister("Polarity", configRegister);
	_tca9535->TCA9535ReadConfig(&configRegister);
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
	if (ESP_OK != _tca9535->TCA9535WriteConfig(&configRegister))
		log_e("failed to write config to tca9535");
	else
		log_i("tca9535 config written!");

	_tca9535->TCA9535ReadConfig(&configRegister);
	dumpRegister("Config", configRegister);
}

void FocusMotor::setup()
{
	// setup the pins
	for (int i = 0; i < data.size(); i++)
	{
		data[i] = new MotorData();
	}

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
	if (pinConfig.useFastAccelStepper)
	{
		if (pinConfig.I2C_SCL > 0)
		{
			init_tca();
			faccel.setExternalCallForPin(externalPinCallback);
		}
		faccel.data = data;
		faccel.setupFastAccelStepper();
	}
	else
	{
		if (pinConfig.I2C_SCL > 0)
		{
			init_tca();
			accel.setExternalCallForPin(externalPinCallback);
		}
		accel.data = data;
		accel.setupAccelStepper();
	}
}

// dont use it, it get no longer triggered from modulehandler
void FocusMotor::loop()
{
	if (pinConfig.useFastAccelStepper)
	{
		// checks if a stepper is still running
		for (int i = 0; i < data.size(); i++)
		{
			bool isRunning = faccel.isRunning(i);
			if (!isRunning && !data[i]->stopped)
			{
				// Only send the information when the motor is halting
				log_d("Sending motor pos %i", i);
				stopStepper(i);
				sendMotorPos(i, 0);
			}
		}
	}
}

void FocusMotor::sendMotorPos(int i, int arraypos)
{
	cJSON *root = cJSON_CreateObject();
	cJSON *stprs = cJSON_CreateArray();
	cJSON_AddItemToObject(root, key_steppers, stprs);
	cJSON *item = cJSON_CreateObject();
	cJSON_AddItemToArray(stprs, item);
	cJSON_AddNumberToObject(item, key_stepperid, i);
	cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
	cJSON_AddNumberToObject(item, "isDone", true);
	arraypos++;

	if (moduleController.get(AvailableModules::wifi) != nullptr)
	{
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->sendJsonWebSocketMsg(root);
	}
	// print result - will that work in the case of an xTask?
	Serial.println("++");
	char *s = cJSON_Print(root);
	Serial.println(s);
	free(s);
	Serial.println();
	Serial.println("--");

	preferences.begin("motor-positions", false);
	preferences.putLong(("motor" + String(i)).c_str(), data[i]->currentPosition);
	preferences.end();
}

void FocusMotor::stopStepper(int i)
{
	log_i("Stop Stepper:%i", i);
	if (pinConfig.useFastAccelStepper)
		faccel.stopFastAccelStepper(i);
	else
		accel.stopAccelStepper(i);
}

void FocusMotor::disableEnablePin(int i)
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

void FocusMotor::enableEnablePin(int i)
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

void FocusMotor::setPosition(Stepper s, int pos)
{
	if (pinConfig.useFastAccelStepper)
	{
		faccel.setPosition(s, pos);
	}
}
