#include "../../config.h"
#include "Rotator.h"

namespace RestApi
{
	void Rotator_act()
	{
		serialize(moduleController.get(AvailableModules::motor)->act(deserialize()));
	}

	void Rotator_get()
	{
		serialize(moduleController.get(AvailableModules::motor)->get(deserialize()));
	}
}


void driveMotorXLoop(void *pvParameter)
{
	Rotator *motor = (Rotator *)moduleController.get(AvailableModules::motor);
	motor->driveMotorLoop(Rotators::rX);
}
void driveMotorYLoop(void *pvParameter)
{
	Rotator *motor = (Rotator *)moduleController.get(AvailableModules::motor);
	motor->driveMotorLoop(Rotators::rY);
}
void driveMotorZLoop(void *pvParameter)
{
	Rotator *motor = (Rotator *)moduleController.get(AvailableModules::motor);
	motor->driveMotorLoop(Rotators::rZ);
}
void driveMotorALoop(void *pvParameter)
{
	Rotator *motor = (Rotator *)moduleController.get(AvailableModules::motor);
	motor->driveMotorLoop(Rotators::rA);
}

Rotator::Rotator() : Module() { log_i("ctor"); }
Rotator::~Rotator() { log_i("~ctor"); }

int Rotator::act(DynamicJsonDocument doc)
{
	log_i("motor act");

	serializeJsonPretty(doc, Serial);
	// only enable/disable motors
	if (doc.containsKey(key_isen))
	{
		for (int i = 0; i < steppers.size(); i++)
		{
			// turn on/off holding current
			if (doc[key_isen])
			{
				log_d("enable motor %d", i);
				steppers[i]->enableOutputs();
			}
			else
			{
				log_d("disable motor %d", i);
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

void Rotator::startStepper(int i)
{

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

DynamicJsonDocument Rotator::get(DynamicJsonDocument docin)
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

void Rotator::setup()
{
	// setup the pins
	log_i("Setting Up Motor A,X,Y,Z");
	steppers[Rotators::rA] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.ROTATOR_A_0, pinConfig.ROTATOR_A_1, pinConfig.ROTATOR_A_2, pinConfig.ROTATOR_A_3);
	steppers[Rotators::rX] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.ROTATOR_X_0, pinConfig.ROTATOR_X_1, pinConfig.ROTATOR_X_2, pinConfig.ROTATOR_X_3);
	steppers[Rotators::rY] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.ROTATOR_Y_0, pinConfig.ROTATOR_Y_1, pinConfig.ROTATOR_Y_2, pinConfig.ROTATOR_Y_3);
	steppers[Rotators::rZ] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.ROTATOR_Z_0, pinConfig.ROTATOR_Z_1, pinConfig.ROTATOR_Z_2, pinConfig.ROTATOR_Z_3);
	log_i("Motor A, dir, step: %i, %i, %i, %i", pinConfig.ROTATOR_A_0, pinConfig.ROTATOR_A_1, pinConfig.ROTATOR_A_2, pinConfig.ROTATOR_A_3);
	log_i("Motor X, dir, step: %i, %i, %i, %i", pinConfig.ROTATOR_X_0, pinConfig.ROTATOR_X_1, pinConfig.ROTATOR_X_2, pinConfig.ROTATOR_X_3);
	log_i("Motor Y, dir, step: %i, %i, %i, %i", pinConfig.ROTATOR_Y_0, pinConfig.ROTATOR_Y_1, pinConfig.ROTATOR_Y_2, pinConfig.ROTATOR_Y_3);
	log_i("Motor Z, dir, step: %i, %i, %i, %i", pinConfig.ROTATOR_Z_0, pinConfig.ROTATOR_Z_1, pinConfig.ROTATOR_Z_2, pinConfig.ROTATOR_Z_3);

	for (int i = 0; i < steppers.size(); i++)
	{
		data[i] = new RotatorData();
	}

	/*
	   Motor related settings
	*/

	for (int i = 0; i < steppers.size(); i++)
	{
		steppers[i]->setMaxSpeed(MAX_VELOCITY_A);
		steppers[i]->setAcceleration(DEFAULT_ACCELERATION_A);
		steppers[i]->runToNewPosition(-1);
		steppers[i]->runToNewPosition(1);
		steppers[i]->setCurrentPosition(data[i]->currentPosition);
	}

	if (pinConfig.ROTATOR_A_0 > 0)
		xTaskCreate(&driveMotorALoop, "motor_task_A", 4096, NULL, tskIDLE_PRIORITY, NULL);
	if (pinConfig.ROTATOR_X_0 > 0)
		xTaskCreate(&driveMotorXLoop, "motor_task_X", 4096, NULL, tskIDLE_PRIORITY, NULL);
	if (pinConfig.ROTATOR_Y_0 > 0)
		xTaskCreate(&driveMotorYLoop, "motor_task_Y", 4096, NULL, tskIDLE_PRIORITY, NULL);
	if (pinConfig.ROTATOR_Z_0 > 0)
		xTaskCreate(&driveMotorZLoop, "motor_task_Z", 4096, NULL, tskIDLE_PRIORITY, NULL);

	
}

void Rotator::driveMotorLoop(int stepperid)
{
	AccelStepper *tSteppers = steppers[stepperid];
	RotatorData *tData = data[stepperid];
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
void Rotator::loop()
{

	// Motor logic runs in a background loop => task

	// check if we want to disable motors after timeout
	bool timeoutReached = true;
	for (int i = 0; i < steppers.size(); i++)
	{
		timeoutReached &= ((millis() - data[i]->timeLastActive) > data[i]->timeoutDisable);
	}
}

void Rotator::sendMotorPos(int i, int arraypos)
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

void Rotator::stopAllDrives()
{
	// Immediately stop the motor
	for (int i = 0; i < 4; i++)
	{
		stopStepper(i);
	}
}

void Rotator::stopStepper(int i)
{
	steppers[i]->stop();
	data[i]->isforever = false;
	data[i]->speed = 0;
	data[i]->currentPosition = steppers[i]->currentPosition();
	data[i]->stopped = true;
}

void Rotator::startAllDrives()
{
	for (int i = 0; i < steppers.size(); i++)
	{
		startStepper(i);
	}
}

