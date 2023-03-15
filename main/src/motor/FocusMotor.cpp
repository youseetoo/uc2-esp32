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
		for (int i = 0; i < motor->steppers.size(); i++)
		{
			if (!motor->data[i]->stopped)
			{
				motor->sendMotorPos(i, arraypos);
			}
		}
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}
}

void driveMotorXLoop(void *pvParameter)
{
	FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
	motor->driveMotorLoop(Stepper::X);
}
void driveMotorYLoop(void *pvParameter)
{
	FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
	motor->driveMotorLoop(Stepper::Y);
}
void driveMotorZLoop(void *pvParameter)
{
	FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
	motor->driveMotorLoop(Stepper::Z);
}
void driveMotorALoop(void *pvParameter)
{
	FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
	motor->driveMotorLoop(Stepper::A);
}

FocusMotor::FocusMotor() : Module() { log_i("ctor"); }
FocusMotor::~FocusMotor() { log_i("~ctor"); }

int FocusMotor::act(DynamicJsonDocument doc)
{
	log_i("motor act");

	// only enable/disable motors
	if (doc.containsKey(key_isen))
	{
		for (int i = 0; i < steppers.size(); i++)
		{
			// turn on/off holding current
			if (doc[key_isen])
			{
				steppers[i]->enableOutputs();
			}
			else
			{
				steppers[i]->disableOutputs();
			}
		}
		return 1;
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
					data[s]->isaccelerated = (bool)doc[key_motor][key_steppers][i][key_isaccel];
				else // we always switch off acceleration if not set
					data[s]->isaccelerated = false;

				if (doc[key_motor][key_steppers][i].containsKey(key_acceleration))
					data[s]->acceleration = doc[key_motor][key_steppers][i][key_acceleration];
				else
					data[s]->acceleration = 200;

				// make sure speed and position are pointing in the same direction
				if (data[s]->absolutePosition)
				{
					// if an absolute position occurs, wehave to compute its direction (positive or negative)
					if (data[s]->targetPosition > steppers[s]->currentPosition())
						data[s]->speed = abs(data[s]->speed);
					else if (data[s]->targetPosition < steppers[s]->currentPosition())
						data[s]->speed = -abs(data[s]->speed);
					else // 0
						data[s]->speed = 0;
				}
				else
				{
					// if relativce position the direction and speed sign have to match
					if (data[s]->targetPosition > 0)
						data[s]->speed = abs(data[s]->speed);
					else if (data[s]->targetPosition < 0)
						data[s]->speed = -abs(data[s]->speed);
					else // 0
						data[s]->speed = 0;
				}

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
	enableEnablePin(i);
	steppers[i]->setMaxSpeed(data[i]->maxspeed);
	steppers[i]->setAcceleration(data[i]->acceleration);
	if (data[i]->absolutePosition == 1)
	{
		// absolute position coordinates
		steppers[i]->moveTo(data[i]->targetPosition);
	}
	else if (!data[i]->isforever)
	{
		// relative coordinates
		steppers[i]->move(data[i]->targetPosition);
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
		for (int i = 0; i < steppers.size(); i++)
		{
			// update position and push it to the json
			data[i]->currentPosition = 1;
			doc[key_motor][key_steppers][i][key_stepperid] = i;
			doc[key_motor][key_steppers][i][key_position] = steppers[i]->currentPosition();
		}
		return doc;
	}

	// only return if motor is still busy
	if (docin.containsKey(key_stopped))
	{
		docin.clear();
		for (int i = 0; i < steppers.size(); i++)
		{
			// update position and push it to the json
			doc[key_motor][key_steppers][i][key_stopped] = !data[i]->stopped;
		}
		return doc;
	}

	// return the whole config
	docin.clear();
	for (int i = 0; i < steppers.size(); i++)
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
	steppers[Stepper::A] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_A_STEP, pinConfig.MOTOR_A_DIR);
	steppers[Stepper::X] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_X_STEP, pinConfig.MOTOR_X_DIR);
	steppers[Stepper::Y] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Y_STEP, pinConfig.MOTOR_Y_DIR);
	steppers[Stepper::Z] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Z_STEP, pinConfig.MOTOR_Z_DIR);
	log_i("Motor A, dir, step: %i, %i", pinConfig.MOTOR_A_DIR, pinConfig.MOTOR_A_STEP);
	log_i("Motor X, dir, step: %i, %i", pinConfig.MOTOR_X_DIR, pinConfig.MOTOR_X_STEP);
	log_i("Motor Y, dir, step: %i, %i", pinConfig.MOTOR_Y_DIR, pinConfig.MOTOR_Y_STEP);
	log_i("Motor Z, dir, step: %i, %i", pinConfig.MOTOR_Z_DIR, pinConfig.MOTOR_Z_STEP);

	for (int i = 0; i < steppers.size(); i++)
	{
		data[i] = new MotorData();
	}

	/*
	   Motor related settings
	*/

	enableEnablePin(0);
	for (int i = 0; i < steppers.size(); i++)
	{
		steppers[i]->setMaxSpeed(MAX_VELOCITY_A);
		steppers[i]->setAcceleration(DEFAULT_ACCELERATION_A);
		steppers[i]->runToNewPosition(-1);
		steppers[i]->runToNewPosition(1);
		steppers[i]->setCurrentPosition(data[i]->currentPosition);
	}
	disableEnablePin(0);

	if (pinConfig.MOTOR_A_DIR > 0)
		xTaskCreate(&driveMotorALoop, "motor_task_A", 4096, NULL, tskIDLE_PRIORITY, NULL);
	if (pinConfig.MOTOR_X_DIR > 0)
		xTaskCreate(&driveMotorXLoop, "motor_task_X", 4096, NULL, tskIDLE_PRIORITY, NULL);
	if (pinConfig.MOTOR_Y_DIR > 0)
		xTaskCreate(&driveMotorYLoop, "motor_task_Y", 4096, NULL, tskIDLE_PRIORITY, NULL);
	if (pinConfig.MOTOR_Z_DIR > 0)
		xTaskCreate(&driveMotorZLoop, "motor_task_Z", 4096, NULL, tskIDLE_PRIORITY, NULL);

	//xTaskCreate(&sendUpdateToClients, "motor_websocket_task", 2048, NULL, 5, NULL);
}

void FocusMotor::driveMotorLoop(int stepperid)
{
	AccelStepper *tSteppers = steppers[stepperid];
	MotorData *tData = data[stepperid];
	int arraypos = 0;
	// log_i("start motor loop:%i", stepperid);
	tSteppers->setMaxSpeed(tData->maxspeed);
	for (;;)
	{

		if (tData->isforever)
		{
			tSteppers->setSpeed(tData->speed);
			tSteppers->runSpeed();
		}
		else
		{
			if (tData->isaccelerated == 1)
			{
				tSteppers->run();
			}
			else
			{
				tSteppers->setSpeed(tData->speed);
				tSteppers->runSpeed();
			}
			// checks if a stepper is still running
			if (tSteppers->distanceToGo() == 0 && !tData->stopped)
			{
				// if not turn it off
				stopStepper(stepperid);

				sendMotorPos(stepperid, arraypos);

				// initialte a disabling of the motors after the timeout has reached
				tData->timeLastActive	= millis();
			}
		}
		tData->currentPosition = tSteppers->currentPosition();
		const TickType_t xDelay = 0;// / portTICK_PERIOD_MS;
		//vTaskDelay(xDelay);
	}
}

// dont use it, it get no longer triggered from modulehandler
void FocusMotor::loop()
{

	// Motor logic runs in a background loop => task

	// check if we want to disable motors after timeout
	bool timeoutReached = true;
	for (int i = 0; i < steppers.size(); i++)
	{
		timeoutReached &= ((millis() - data[i]->timeLastActive) > data[i]->timeoutDisable);
		if (timeoutReached)
		{ // only if all of the motors have reached their timeout, disable the enable pin
			disableEnablePin(-1);
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
	steppers[i]->stop();
	data[i]->isforever = false;
	data[i]->speed = 0;
	data[i]->currentPosition = steppers[i]->currentPosition();
	data[i]->stopped = true;
	if (not data[i]->isEnable)
		disableEnablePin(i);
}

void FocusMotor::startAllDrives()
{
	for (int i = 0; i < steppers.size(); i++)
	{
		startStepper(i);
	}
}

void FocusMotor::disableEnablePin(int i)
{

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
}

void FocusMotor::enableEnablePin(int i)
{
	if (!power_enable)
	{
		pinMode(pinConfig.MOTOR_ENABLE, OUTPUT);
		digitalWrite(pinConfig.MOTOR_ENABLE, HIGH ^ pinConfig.MOTOR_ENABLE_INVERTED);
		power_enable = true;
	}
}
