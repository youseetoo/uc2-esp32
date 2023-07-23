#include "../../config.h"
#include "FocusMotor.h"

namespace RestApi
{
	void FocusMotor_act()
	{
		serialize(moduleController.get(AvailableModules::motor)->act(deserialize()));
	}

	void FocusMotor_get()
	{
		serialize(moduleController.get(AvailableModules::motor)->get(deserialize()));
	}
}

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
	FocusMotor * m = (FocusMotor*)moduleController.get(AvailableModules::motor);
	return m->setExternalPin(pin,value);
}


FocusMotor::FocusMotor() : Module() { log_i("ctor"); }
FocusMotor::~FocusMotor() { log_i("~ctor"); }

int FocusMotor::act(DynamicJsonDocument doc)
{
	log_i("motor act");

	serializeJsonPretty(doc, Serial);
	// only enable/disable motors
	// {"task":"/motor_act", "isen":1, "isenauto":1}
	if (doc.containsKey(key_isen))
	{
		for (int i = 0; i < faststeppers.size(); i++)
		{
			// turn on/off holding current
			if (doc[key_isen])
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
	if (doc.containsKey(key_isenauto))
	{
		for (int i = 0; i < faststeppers.size(); i++)
		{
			// turn on/off holding current
			if (doc[key_isenauto])
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

	// do everything else
	if (doc.containsKey(key_motor))
	{
		if (doc[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < doc[key_motor][key_steppers].size(); i++)
			{

				Stepper s = static_cast<Stepper>(doc[key_motor][key_steppers][i][key_stepperid]);

				if (doc[key_motor][key_steppers][i].containsKey(key_speed))
					data[s]->speed = doc[key_motor][key_steppers][i][key_speed];
				else
					data[s]->speed = 0;

				if (doc[key_motor][key_steppers][i].containsKey(key_isen))
					data[s]->isEnable = doc[key_motor][key_steppers][i][key_isen];
				else
					data[s]->isEnable = 0;

				if (doc[key_motor][key_steppers][i].containsKey(key_position))
					data[s]->targetPosition = doc[key_motor][key_steppers][i][key_position];

				if (doc[key_motor][key_steppers][i].containsKey(key_isforever))
					data[s]->isforever = doc[key_motor][key_steppers][i][key_isforever];
				else // if not set, set to false
					data[s]->isforever = false;

				if (doc[key_motor][key_steppers][i].containsKey(key_isabs))
					data[s]->absolutePosition = doc[key_motor][key_steppers][i][key_isabs];
				else // we always set absolute position to false if not set
					data[s]->absolutePosition = false;

				if (doc[key_motor][key_steppers][i].containsKey(key_acceleration))
					data[s]->acceleration = doc[key_motor][key_steppers][i][key_acceleration];
				else
					data[s]->acceleration = DEFAULT_ACCELERATION;

				log_i("start stepper (act): motor:%i, index: %i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i, isacceleration: %i, acceleration: %i", s, i, data[s]->isforever, data[s]->speed, data[s]->maxspeed, data[s]->targetPosition, data[s]->absolutePosition, data[s]->isaccelerated, data[s]->acceleration);

				if (doc[key_motor][key_steppers][i].containsKey(key_isstop))
					stopStepper(s);
				else
				{
					startStepper(s);
				}
			}
		}
	}
	return 1;
}

void FocusMotor::startStepper(int i)
{
	// enableEnablePin(i);
	if (faststeppers[i] == nullptr)
	{
		return;
	}
	int speed =data[i]->speed;
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

DynamicJsonDocument FocusMotor::get(DynamicJsonDocument docin)
{
	log_i("get motor");
	DynamicJsonDocument doc(4096); // StaticJsonDocument<1024> doc; // create return doc
	// only return the position of the stepper
	if (docin.containsKey(key_position))
	{
		docin.clear();
		for (int i = 0; i < faststeppers.size(); i++)
		{
			// update position and push it to the json
			data[i]->currentPosition = 1;
			doc[key_motor][key_steppers][i][key_stepperid] = i;
			doc[key_motor][key_steppers][i][key_position] = faststeppers[i]->getCurrentPosition();
		}
		return doc;
	}

	// only return if motor is still busy
	if (docin.containsKey(key_stopped))
	{
		docin.clear();
		for (int i = 0; i < faststeppers.size(); i++)
		{
			// update position and push it to the json
			doc[key_motor][key_steppers][i][key_stopped] = !data[i]->stopped;
		}
		return doc;
	}

	// return the whole config
	docin.clear();
	for (int i = 0; i < faststeppers.size(); i++)
	{
		doc[key_steppers][i][key_stepperid] = i;
		doc[key_steppers][i][key_position] = data[i]->currentPosition;
	}
	return doc;
}


bool FocusMotor::setExternalPin(uint8_t pin, uint8_t value) {
	// This example returns the previous value of the output.
	// Consequently, FastAccelStepper needs to call setExternalPin twice
	// in order to successfully change the output value.
	pin = pin & ~PIN_EXTERNAL_FLAG;
	TCA9535_Register configRegister;
	if(pin == 100) //enable
		configRegister.Port.P0.bit.Bit0 = value;
	if(pin == 101)//x
		configRegister.Port.P0.bit.Bit1 = value;
	if(pin == 102)//y
		configRegister.Port.P0.bit.Bit2 = value;
	if(pin == 103)//z
		configRegister.Port.P0.bit.Bit3 = value;
	if(pin == 104)//a
		configRegister.Port.P0.bit.Bit4 = value;
	log_i("external pin cb for pin:%d value:%d",pin, value);
	return value;
}

void FocusMotor::setup()
{
	// setup the pins
	log_i("Setting Up Motor A,X,Y,Z");
	engine.init();
	if(pinConfig.I2C_SCL > 0)
	{
		engine.setExternalCallForPin(&externalPinCallback);
		_tca9535 = new tca9535();
		
		if(ESP_OK != _tca9535->TCA9535Init(pinConfig.I2C_SCL, pinConfig.I2C_SDA, pinConfig.I2C_ADD))
			log_i("failed to init tca9535");
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
		data[Stepper::A]->currentPosition = preferences.getLong(("motor"+String(Stepper::A)).c_str());
	if (pinConfig.MOTOR_X_DIR > 0)
		data[Stepper::X]->currentPosition = preferences.getLong(("motor"+String(Stepper::X)).c_str());
	if (pinConfig.MOTOR_Y_DIR > 0)
		data[Stepper::Y]->currentPosition = preferences.getLong(("motor"+String(Stepper::Y)).c_str());
	if (pinConfig.MOTOR_Z_DIR > 0)
		data[Stepper::Z]->currentPosition = preferences.getLong(("motor"+String(Stepper::Z)).c_str());
	preferences.end();

	// setup the stepper A
	if (pinConfig.MOTOR_A_STEP > 0)
	{
		log_i("Motor A, dir, step: %i, %i", pinConfig.MOTOR_A_DIR, pinConfig.MOTOR_A_STEP);
		faststeppers[Stepper::A] = engine.stepperConnectToPin(pinConfig.MOTOR_A_STEP);
		faststeppers[Stepper::A]->setDirectionPin(pinConfig.MOTOR_A_DIR);
		if (pinConfig.I2C_SCL > 0){
			faststeppers[Stepper::A]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
			faststeppers[Stepper::A]->setDirectionPin(104 | PIN_EXTERNAL_FLAG);
		}
		else{
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
	if (pinConfig.MOTOR_X_STEP > 0)
	{
		log_i("Motor X, dir, step: %i, %i", pinConfig.MOTOR_X_DIR, pinConfig.MOTOR_X_STEP);
		faststeppers[Stepper::X] = engine.stepperConnectToPin(pinConfig.MOTOR_X_STEP);
		
		if (pinConfig.I2C_SCL > 0){
			faststeppers[Stepper::X]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
			faststeppers[Stepper::X]->setDirectionPin(101 | PIN_EXTERNAL_FLAG);
		}
		else {
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
	if (pinConfig.MOTOR_Y_STEP > 0)
	{
		log_i("Motor Y, dir, step: %i, %i", pinConfig.MOTOR_Y_DIR, pinConfig.MOTOR_Y_STEP);
		faststeppers[Stepper::Y] = engine.stepperConnectToPin(pinConfig.MOTOR_Y_STEP);
		
		if (pinConfig.I2C_SCL > 0){
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
	if (pinConfig.MOTOR_Z_STEP > 0)
	{
		log_i("Motor Z, dir, step: %i, %i", pinConfig.MOTOR_Z_DIR, pinConfig.MOTOR_Z_STEP);
		faststeppers[Stepper::Z] = engine.stepperConnectToPin(pinConfig.MOTOR_Z_STEP);
		
		if (pinConfig.I2C_SCL > 0)
		{
			faststeppers[Stepper::Z]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
			faststeppers[Stepper::Z]->setDirectionPin(103 | PIN_EXTERNAL_FLAG);
		}
		else {
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
			//long distanceToGo = faststeppers[i]->getCurrentPosition() - faststeppers[i]->targetPos();
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
	DynamicJsonDocument doc(4096);
	doc[key_steppers][arraypos][key_stepperid] = i;
	doc[key_steppers][arraypos][key_position] = data[i]->currentPosition;
	doc[key_steppers][arraypos]["isDone"] = true;
	arraypos++;

	if (moduleController.get(AvailableModules::wifi) != nullptr)
	{
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->sendJsonWebSocketMsg(doc);
	}
	// print result - will that work in the case of an xTask?
	Serial.println("++");
	serializeJson(doc, Serial);
	Serial.println();
	Serial.println("--");

	preferences.begin("motor-positions", false);
	preferences.putLong(("motor"+String(i)).c_str(), data[i]->currentPosition);
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
	if(faststeppers[i] == nullptr)
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
		if(faststeppers[i] != nullptr)
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
