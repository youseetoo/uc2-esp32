#include "../../config.h"
#include "FocusMotor.h"
#include "../wifi/WifiController.h"

void sendUpdateToClients(void *p)
{
	for (;;)
	{
		int arraypos = 0;
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		for (int i = 0; i < motor->faststeppers.size(); i++)
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
		for (int i = 0; i < faststeppers.size(); i++)
		{
			if (isen->valueint == 1)
			{
				log_d("enable motor %d - going manual mode!", i);
				faststeppers[i]->enableOutputs();
				faststeppers[i]->setAutoEnable(false);
			}
			else
			{
				log_d("disable motor %d - going manual mode!", i);
				faststeppers[i]->disableOutputs();
				faststeppers[i]->setAutoEnable(false);
			}
		}
	}

	cJSON *autoen = cJSON_GetObjectItemCaseSensitive(doc, key_isenauto);
	if (autoen != NULL)
	{
		for (int i = 0; i < faststeppers.size(); i++)
		{
			// turn on/off holding current
			if (autoen->valueint == 1)
			{
				log_d("enable motor %d - automode on", i);
				faststeppers[i]->setAutoEnable(true);
			}
			else
			{
				log_d("disable motor %d - automode off", i);
				faststeppers[i]->setAutoEnable(false);
			}
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
				int stop = getJsonInt(stp, key_isstop);
				log_i("start stepper (act): motor:%i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i, isacceleration: %i, acceleration: %i", s, data[s]->isforever, data[s]->speed, data[s]->maxspeed, data[s]->targetPosition, data[s]->absolutePosition, data[s]->isaccelerated, data[s]->acceleration);
				if (stop == 0)
					stopStepper(s);
				else
				{
					startStepper(s);
				}
			}
		}else log_i("Motor steppers json is null");
	}
	else log_i("Motor json is null");
	return 1;
}

void FocusMotor::startStepper(int i)
{
	// enableEnablePin(i);
	if (faststeppers[i] == nullptr)
	{
		return;
	}
	int speed = data[i]->speed;
	if (data[i]->speed < 0)
	{
		speed *= -1;
	}

	faststeppers[i]->setSpeedInHz(speed);
	faststeppers[i]->setAcceleration(data[i]->acceleration);

	if (data[i]->isforever)
	{
		// run forver (e.g. PSx or initaited via Serial)
		if (data[i]->speed > 0)
		{
			// run clockwise
			faststeppers[i]->runForward();
		}
		else
		{
			// run counterclockwise
			faststeppers[i]->runBackward();
		}
	}
	else
	{
		if (data[i]->absolutePosition == 1)
		{
			// absolute position coordinates
			faststeppers[i]->moveTo(data[i]->targetPosition, false);
		}
		else if (data[i]->absolutePosition == 0)
		{
			// relative position coordinates
			faststeppers[i]->move(data[i]->targetPosition, false);
		}
		else if (!data[i]->isforever)
		{
			// relative coordinates
		}
	}
	data[i]->stopped = false;
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
		for (int i = 0; i < faststeppers.size(); i++)
		{
			// update position and push it to the json
			data[i]->currentPosition = 1;
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stepperid, i);
			setJsonInt(aritem, key_position, faststeppers[i]->getCurrentPosition());
			cJSON_AddItemToArray(stprs, aritem);
		}
		return doc;
	}

	cJSON *stop = cJSON_GetObjectItemCaseSensitive(docin, key_stopped);
	if (stop != NULL)
	{
		for (int i = 0; i < faststeppers.size(); i++)
		{
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stopped, !data[i]->stopped);
			cJSON_AddItemToArray(stprs, aritem);
		}
		return doc;
	}

	// return the whole config
	for (int i = 0; i < faststeppers.size(); i++)
	{
		data[i]->currentPosition = faststeppers[i]->getCurrentPosition();
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
	TCA9535_Register configRegister;
	if (pin == 100) // enable
		configRegister.Port.P0.bit.Bit0 = value;
	if (pin == 101) // x
		configRegister.Port.P0.bit.Bit1 = value;
	if (pin == 102) // y
		configRegister.Port.P0.bit.Bit2 = value;
	if (pin == 103) // z
		configRegister.Port.P0.bit.Bit3 = value;
	if (pin == 104) // a
		configRegister.Port.P0.bit.Bit4 = value;
	log_i("external pin cb for pin:%d value:%d", pin, value);
	if (ESP_OK != _tca9535->TCA9535WriteOutput(&configRegister))
		log_e("i2c write failed");

	return value;
}

void FocusMotor::setup()
{
	// setup the pins
	log_i("Setting Up Motor A,X,Y,Z");
	engine.init();
	if (pinConfig.I2C_SCL > 0)
	{
		engine.setExternalCallForPin(&externalPinCallback);
		_tca9535 = new tca9535();

		if (ESP_OK != _tca9535->TCA9535Init(pinConfig.I2C_SCL, pinConfig.I2C_SDA, pinConfig.I2C_ADD))
			log_e("failed to init tca9535");
		else
			log_i("tca9535 init!");
	}

	// setup the data
	for (int i = 0; i < faststeppers.size(); i++)
	{
		data[i] = new MotorData();
		faststeppers[i] = nullptr;
	}

	/* restore previously saved motor position values*/
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

	// setup the stepper A
	if (pinConfig.MOTOR_A_STEP >= 0)
	{
		log_i("Motor A, dir, step: %i, %i", pinConfig.MOTOR_A_DIR, pinConfig.MOTOR_A_STEP);
		faststeppers[Stepper::A] = engine.stepperConnectToPin(pinConfig.MOTOR_A_STEP);
		faststeppers[Stepper::A]->setDirectionPin(pinConfig.MOTOR_A_DIR);
		if (pinConfig.I2C_SCL > 0)
		{
			faststeppers[Stepper::A]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
			faststeppers[Stepper::A]->setDirectionPin(104 | PIN_EXTERNAL_FLAG);
		}
		else
		{
			faststeppers[Stepper::A]->setEnablePin(pinConfig.MOTOR_ENABLE);
			faststeppers[Stepper::A]->setDirectionPin(pinConfig.MOTOR_A_DIR);
		}
		faststeppers[Stepper::A]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
		faststeppers[Stepper::A]->setSpeedInHz(MAX_VELOCITY_A);
		faststeppers[Stepper::A]->setAcceleration(DEFAULT_ACCELERATION);
		faststeppers[Stepper::A]->setCurrentPosition(data[Stepper::A]->currentPosition);
		faststeppers[Stepper::A]->move(2);
		faststeppers[Stepper::A]->move(-2);
	}

	// setup the stepper X
	if (pinConfig.MOTOR_X_STEP >= 0)
	{
		log_i("Motor X, dir, step: %i, %i", pinConfig.MOTOR_X_DIR, pinConfig.MOTOR_X_STEP);
		faststeppers[Stepper::X] = engine.stepperConnectToPin(pinConfig.MOTOR_X_STEP);

		if (pinConfig.I2C_SCL > 0)
		{
			faststeppers[Stepper::X]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
			faststeppers[Stepper::X]->setDirectionPin(101 | PIN_EXTERNAL_FLAG);
		}
		else
		{
			faststeppers[Stepper::X]->setEnablePin(pinConfig.MOTOR_ENABLE);
			faststeppers[Stepper::X]->setDirectionPin(pinConfig.MOTOR_X_DIR);
		}
		faststeppers[Stepper::X]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
		faststeppers[Stepper::X]->setSpeedInHz(MAX_VELOCITY_A);
		faststeppers[Stepper::X]->setAcceleration(DEFAULT_ACCELERATION);
		faststeppers[Stepper::X]->setCurrentPosition(data[Stepper::X]->currentPosition);
		faststeppers[Stepper::X]->move(2000);
		faststeppers[Stepper::X]->move(-2000);
	}

	// setup the stepper Y
	if (pinConfig.MOTOR_Y_STEP >= 0)
	{
		log_i("Motor Y, dir, step: %i, %i", pinConfig.MOTOR_Y_DIR, pinConfig.MOTOR_Y_STEP);
		faststeppers[Stepper::Y] = engine.stepperConnectToPin(pinConfig.MOTOR_Y_STEP);

		if (pinConfig.I2C_SCL > 0)
		{
			faststeppers[Stepper::Y]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
			faststeppers[Stepper::Y]->setDirectionPin(102 | PIN_EXTERNAL_FLAG);
		}
		else
		{
			faststeppers[Stepper::Y]->setEnablePin(pinConfig.MOTOR_ENABLE);
			faststeppers[Stepper::Y]->setDirectionPin(pinConfig.MOTOR_Y_DIR);
		}
		faststeppers[Stepper::Y]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
		faststeppers[Stepper::Y]->setSpeedInHz(MAX_VELOCITY_A);
		faststeppers[Stepper::Y]->setAcceleration(DEFAULT_ACCELERATION);
		faststeppers[Stepper::Y]->setCurrentPosition(data[Stepper::Y]->currentPosition);
		faststeppers[Stepper::Y]->move(2);
		faststeppers[Stepper::Y]->move(-2);
	}

	// setup the stepper Z
	if (pinConfig.MOTOR_Z_STEP >= 0)
	{
		log_i("Motor Z, dir, step: %i, %i", pinConfig.MOTOR_Z_DIR, pinConfig.MOTOR_Z_STEP);
		faststeppers[Stepper::Z] = engine.stepperConnectToPin(pinConfig.MOTOR_Z_STEP);

		if (pinConfig.I2C_SCL > 0)
		{
			faststeppers[Stepper::Z]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
			faststeppers[Stepper::Z]->setDirectionPin(103 | PIN_EXTERNAL_FLAG);
		}
		else
		{
			faststeppers[Stepper::Z]->setEnablePin(pinConfig.MOTOR_ENABLE);
			faststeppers[Stepper::Z]->setDirectionPin(pinConfig.MOTOR_Z_DIR);
		}
		faststeppers[Stepper::Z]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
		faststeppers[Stepper::Z]->setSpeedInHz(MAX_VELOCITY_A);
		faststeppers[Stepper::Z]->setAcceleration(DEFAULT_ACCELERATION);
		faststeppers[Stepper::Z]->setCurrentPosition(data[Stepper::Z]->currentPosition);
		faststeppers[Stepper::Z]->move(2);
		faststeppers[Stepper::Z]->move(-2);
	}
}

// dont use it, it get no longer triggered from modulehandler
void FocusMotor::loop()
{
	// checks if a stepper is still running
	for (int i = 0; i < faststeppers.size(); i++)
	{
		if (faststeppers[i] != NULL)
		{
			// long distanceToGo = faststeppers[i]->getCurrentPosition() - faststeppers[i]->targetPos();
			bool isRunning = faststeppers[i]->isRunning();
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
	char * s = cJSON_Print(root);
	Serial.println(s);
	free(s);
	Serial.println();
	Serial.println("--");

	preferences.begin("motor-positions", false);
	preferences.putLong(("motor" + String(i)).c_str(), data[i]->currentPosition);
	preferences.end();
}

void FocusMotor::stopAllDrives()
{
	// Immediately stop the motor
	for (int i = 0; i < 4; i++)
	{
		stopStepper(i);
	}
}

void FocusMotor::stopStepper(int i)
{
	if (faststeppers[i] == nullptr)
		return;
	faststeppers[i]->forceStop();
	faststeppers[i]->stopMove();
	data[i]->isforever = false;
	data[i]->speed = 0;
	data[i]->currentPosition = faststeppers[i]->getCurrentPosition();
	data[i]->stopped = true;
}

void FocusMotor::startAllDrives()
{
	for (int i = 0; i < faststeppers.size(); i++)
	{
		if (faststeppers[i] != nullptr)
			startStepper(i);
	}
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
