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

				if (doc[key_motor][key_steppers][i].containsKey(key_isaccel))
				{
					data[s]->isaccelerated = (bool)doc[key_motor][key_steppers][i][key_isaccel];
					data[s]->acceleration = 4294967295;
				}
				else
				{
					// we always switch off acceleration if not set
					data[s]->isaccelerated = false;
					data[s]->acceleration = DEFAULT_ACCELERATION;
				}

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
	//enableEnablePin(i);
	faststeppers[i]->setSpeedInHz(data[i]->speed);
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

void FocusMotor::setup()
{
	// setup the pins
	log_i("Setting Up Motor A,X,Y,Z");
	engine.init();

	// setup the data
	for (int i = 0; i < faststeppers.size(); i++)
	{
		data[i] = new MotorData();
	}

	// setup the stepper A
	log_i("Motor A, dir, step: %i, %i", pinConfig.MOTOR_A_DIR, pinConfig.MOTOR_A_STEP);
	faststeppers[Stepper::A] = NULL;
	faststeppers[Stepper::A] = engine.stepperConnectToPin(pinConfig.MOTOR_A_STEP);
	faststeppers[Stepper::A]->setDirectionPin(pinConfig.MOTOR_A_DIR);
	faststeppers[Stepper::A]->setEnablePin(pinConfig.MOTOR_ENABLE);
	faststeppers[Stepper::A]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
	faststeppers[Stepper::A]->setSpeedInHz(MAX_VELOCITY_A);
	faststeppers[Stepper::A]->setAcceleration(DEFAULT_ACCELERATION);
	faststeppers[Stepper::A]->setCurrentPosition(data[Stepper::A]->currentPosition);
	faststeppers[Stepper::A]->move(2);
	faststeppers[Stepper::A]->move(-2);

	// setup the stepper X
	log_i("Motor X, dir, step: %i, %i", pinConfig.MOTOR_X_DIR, pinConfig.MOTOR_X_STEP);
	faststeppers[Stepper::X] = NULL;
	faststeppers[Stepper::X] = engine.stepperConnectToPin(pinConfig.MOTOR_X_STEP);
	faststeppers[Stepper::X]->setDirectionPin(pinConfig.MOTOR_X_DIR);
	faststeppers[Stepper::X]->setEnablePin(pinConfig.MOTOR_ENABLE);
	faststeppers[Stepper::X]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
	faststeppers[Stepper::X]->setSpeedInHz(MAX_VELOCITY_A);
	faststeppers[Stepper::X]->setAcceleration(DEFAULT_ACCELERATION);
	faststeppers[Stepper::X]->setCurrentPosition(data[Stepper::X]->currentPosition);
	faststeppers[Stepper::X]->move(2);
	faststeppers[Stepper::X]->move(-2);

	// setup the stepper Y
	log_i("Motor Y, dir, step: %i, %i", pinConfig.MOTOR_Y_DIR, pinConfig.MOTOR_Y_STEP);
	faststeppers[Stepper::Y] = NULL;
	faststeppers[Stepper::Y] = engine.stepperConnectToPin(pinConfig.MOTOR_Y_STEP);
	faststeppers[Stepper::Y]->setDirectionPin(pinConfig.MOTOR_Y_DIR);
	faststeppers[Stepper::Y]->setEnablePin(pinConfig.MOTOR_ENABLE);
	faststeppers[Stepper::Y]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
	faststeppers[Stepper::Y]->setSpeedInHz(MAX_VELOCITY_A);
	faststeppers[Stepper::Y]->setAcceleration(DEFAULT_ACCELERATION);
	faststeppers[Stepper::Y]->setCurrentPosition(data[Stepper::Y]->currentPosition);
	faststeppers[Stepper::Y]->move(2);
	faststeppers[Stepper::Y]->move(-2);

	// setup the stepper Z
	log_i("Motor Z, dir, step: %i, %i", pinConfig.MOTOR_Z_DIR, pinConfig.MOTOR_Z_STEP);
	faststeppers[Stepper::Z] = NULL;
	faststeppers[Stepper::Z] = engine.stepperConnectToPin(pinConfig.MOTOR_Z_STEP);
	faststeppers[Stepper::Z]->setDirectionPin(pinConfig.MOTOR_Z_DIR);
	faststeppers[Stepper::Z]->setEnablePin(pinConfig.MOTOR_ENABLE);
	faststeppers[Stepper::Z]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
	faststeppers[Stepper::Z]->setSpeedInHz(MAX_VELOCITY_A);
	faststeppers[Stepper::Z]->setAcceleration(DEFAULT_ACCELERATION);
	faststeppers[Stepper::Z]->setCurrentPosition(data[Stepper::Z]->currentPosition);
	faststeppers[Stepper::Z]->move(2);
	faststeppers[Stepper::Z]->move(-2);
}

// dont use it, it get no longer triggered from modulehandler
void FocusMotor::loop()
{	
	// checks if a stepper is still running
	for (int i = 0; i < faststeppers.size(); i++)
	{
		long distanceToGo = faststeppers[i]->getCurrentPosition() - faststeppers[i]->targetPos();
		bool isRunning = faststeppers[i]->isRunning();
		if (not isRunning && not data[i]->stopped)
		{
			// Only send the information when the motor is halting
			log_d("Sending motor pos %i", i);
			stopStepper(i);
			sendMotorPos(i, 0);
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
	faststeppers[i]->forceStop();
	faststeppers[i]->stopMove();
	data[i]->isforever = false;
	data[i]->speed = 0;
	data[i]->currentPosition = faststeppers[i]->getCurrentPosition();
	data[i]->stopped = true;

/*	if (not data[i]->isEnable)
		disableEnablePin(i);*/
}

void FocusMotor::startAllDrives()
{
	for (int i = 0; i < faststeppers.size(); i++)
	{
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
