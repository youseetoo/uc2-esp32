#include "Rotator.h"
#include "../wifi/WifiController.h"
#include "../config/ConfigController.h"
#include "../../ModuleController.h"
#include "../../PinConfig.h"
#include "../motor/FocusMotor.h"

Rotator::Rotator() : Module() { log_i("ctor"); }
Rotator::~Rotator() { log_i("~ctor"); }

int Rotator::act(cJSON *doc)
{
	log_i("motor act");

	cJSON *isen = cJSON_GetObjectItemCaseSensitive(doc, key_isen);
	if (isen != NULL)
	{
		for (int i = 0; i < steppers.size(); i++)
		{
			if (isen->valueint == 1)
			{
				log_d("enable motor %d - going manual mode!", i);
				steppers[i]->enableOutputs();
			}
			else
			{
				log_d("disable motor %d - going manual mode!", i);
				steppers[i]->disableOutputs();
			}
		}
	}

	cJSON *mot = cJSON_GetObjectItemCaseSensitive(doc, key_motor);
	if (mot != NULL)
	{
		cJSON *stprs = cJSON_GetObjectItemCaseSensitive(doc, key_steppers);
		cJSON *stp = NULL;
		if (stprs != NULL)
		{
			cJSON_ArrayForEach(stp, stprs)
			{
				Stepper s = static_cast<Stepper>(cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)));
				data[s]->speed=getJsonInt(stp, key_speed);
				data[s]->isEnable = getJsonInt(stp, key_isen);
				data[s]->targetPosition=getJsonInt(stp, key_position);
				data[s]->isforever=getJsonInt(stp, key_isforever);
				data[s]->absolutePosition=getJsonInt(stp, key_isabs);
				data[s]->acceleration=getJsonInt(stp, key_acceleration);
				data[s]->isaccelerated =getJsonInt(stp, key_isaccel);
				int stop = getJsonInt(stp, key_isstop);
				
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
				log_i("start stepper (act): motor:%i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i, isacceleration: %i, acceleration: %i", s, data[s]->isforever, data[s]->speed, data[s]->maxspeed, data[s]->targetPosition, data[s]->absolutePosition, data[s]->isaccelerated, data[s]->acceleration);
				if (stop == 0)
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
	log_d("start rotator: motor:%i, index: %i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i, isacceleration: %i, acceleration: %i", i, data[i]->isforever, data[i]->speed, data[i]->maxspeed, data[i]->targetPosition, data[i]->absolutePosition, data[i]->isaccelerated, data[i]->acceleration);
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

cJSON *Rotator::get(cJSON *docin)
{
	log_i("get rotator");
	log_i("get motor");
	cJSON *doc = cJSON_CreateObject();
	cJSON *pos = cJSON_GetObjectItemCaseSensitive(docin, key_position);
	cJSON *mot = cJSON_CreateObject();
	cJSON_AddItemToObject(doc, key_motor, mot);
	cJSON *stprs = cJSON_CreateArray();
	cJSON_AddItemToObject(mot, key_steppers, stprs);
	if (pos != NULL)
	{
		for (int i = 0; i < steppers.size(); i++)
		{
			// update position and push it to the json
			data[i]->currentPosition = 1;
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stepperid, i);
			setJsonInt(aritem, key_position, steppers[i]->currentPosition());
			cJSON_AddItemToArray(stprs, aritem);
		}
		return doc;
	}

	cJSON *stop = cJSON_GetObjectItemCaseSensitive(docin, key_stopped);
	if (stop != NULL)
	{
		for (int i = 0; i < steppers.size(); i++)
		{
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stopped, !data[i]->stopped);
			cJSON_AddItemToArray(stprs, aritem);
		}
		return doc;
	}

	// return the whole config
	for (int i = 0; i < steppers.size(); i++)
	{
		data[i]->currentPosition = steppers[i]->currentPosition();
		cJSON *aritem = cJSON_CreateObject();
		setJsonInt(aritem, key_stepperid, i);
		setJsonInt(aritem, key_position, data[i]->currentPosition);
		cJSON_AddItemToArray(stprs, aritem);
	}
	return doc;
}

void Rotator::setup()
{
	// setup the pins
	log_i("Setting Up Motor A,X,Y,Z");
	if (pinConfig.ROTATOR_A_0 > 0)
		steppers[Rotators::rA] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.ROTATOR_A_0, pinConfig.ROTATOR_A_1, pinConfig.ROTATOR_A_2, pinConfig.ROTATOR_A_3);
	if (pinConfig.ROTATOR_X_0 > 0)
		steppers[Rotators::rX] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.ROTATOR_X_0, pinConfig.ROTATOR_X_1, pinConfig.ROTATOR_X_2, pinConfig.ROTATOR_X_3);
	if (pinConfig.ROTATOR_Y_0 > 0)
		steppers[Rotators::rY] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.ROTATOR_Y_0, pinConfig.ROTATOR_Y_1, pinConfig.ROTATOR_Y_2, pinConfig.ROTATOR_Y_3);
	if (pinConfig.ROTATOR_Z_0 > 0)
		steppers[Rotators::rZ] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.ROTATOR_Z_0, pinConfig.ROTATOR_Z_1, pinConfig.ROTATOR_Z_2, pinConfig.ROTATOR_Z_3);

	log_i("Rotator A, dir, step: %i, %i, %i, %i", pinConfig.ROTATOR_A_0, pinConfig.ROTATOR_A_1, pinConfig.ROTATOR_A_2, pinConfig.ROTATOR_A_3);
	log_i("Rotator X, dir, step: %i, %i, %i, %i", pinConfig.ROTATOR_X_0, pinConfig.ROTATOR_X_1, pinConfig.ROTATOR_X_2, pinConfig.ROTATOR_X_3);
	log_i("Rotator Y, dir, step: %i, %i, %i, %i", pinConfig.ROTATOR_Y_0, pinConfig.ROTATOR_Y_1, pinConfig.ROTATOR_Y_2, pinConfig.ROTATOR_Y_3);
	log_i("Rotator Z, dir, step: %i, %i, %i, %i", pinConfig.ROTATOR_Z_0, pinConfig.ROTATOR_Z_1, pinConfig.ROTATOR_Z_2, pinConfig.ROTATOR_Z_3);

	for (int i = 0; i < steppers.size(); i++)
	{
		data[i] = new RotatorData();
	}

	/*
	   Motor related settings
	*/

	// setting default values
	for (int i = 0; i < steppers.size(); i++)
	{
		if (steppers[i])
		{
			log_d("setting default values for motor %i", i);
			steppers[i]->setMaxSpeed(MAX_VELOCITY_A);
			steppers[i]->setAcceleration(DEFAULT_ACCELERATION_A);
			steppers[i]->runToNewPosition(-1);
			steppers[i]->runToNewPosition(1);
			log_d("1");

			steppers[i]->setCurrentPosition(data[i]->currentPosition);
		}
	}
}

void Rotator::driveMotorLoop(int stepperid)
{
	// log_d("driveMotorLoop %i", stepperid);
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

				// sendRotatorPos(stepperid, arraypos);

				// initialte a disabling of the motors after the timeout has reached
				tData->timeLastActive = millis();
			}
		}
		tData->currentPosition = tSteppers->currentPosition();
		const TickType_t xDelay = 0; // / portTICK_PERIOD_MS;
									 // vTaskDelay(xDelay);
	}
}

// dont use it, it get no longer triggered from modulehandler
void Rotator::loop()
{
	int arraypos = 0;

	// iterate over all available motors
	for (int i = 0; i < steppers.size(); i++)
	{

		// move motor only if available
		if (steppers[i] != nullptr)
		{
			steppers[i]->setSpeed(data[i]->speed);
			steppers[i]->setMaxSpeed(data[i]->maxspeed);

			// run forever
			if (data[i]->isforever)
			{
				steppers[i]->runSpeed();
			}
			else // run certain steps
			{
				// checks if a stepper is still running
				if (steppers[i]->distanceToGo() == 0 && !data[i]->stopped)
				{
					log_i("stop stepper:%i", i);
					// if not turn it off
					stopStepper(i);
					sendRotatorPos(i, arraypos);
					/*
					if (pins[i]->max_position != 0 || pins[i]->min_position != 0)
					{
						pins[i]->current_position = steppers[i]->currentPosition();
						Config::setMotorPinConfig(pins);
					}
					*/
				}

				// run at constant speed
				if (data[i]->isaccelerated)
				{
					// Serial.println("Speed (accel) /data" + String(steppers[i]->speed())+"/"+String(data[i]->speed));
					//  Serial.println("Distance to go (accel)" + String(i) + " - "+ String(steppers[i]->distanceToGo()));
					steppers[i]->run();
				}
				else // run accelerated
				{
					// Serial.println("Distance to go " + String(i) + " - "+ String(steppers[i]->distanceToGo()));
					// Serial.println("Speed (non accel) /data" + String(steppers[i]->speed())+"/"+String(data[i]->speed));
					steppers[i]->runSpeedToPosition();
				}
			}
		}
	}
}

void Rotator::sendRotatorPos(int i, int arraypos)
{
	cJSON * root = cJSON_CreateObject();
	cJSON * stprs = cJSON_CreateArray();
	cJSON_AddItemToObject(root,key_steppers, stprs);
	cJSON * item = cJSON_CreateObject();
	cJSON_AddItemToArray(stprs,item);
	cJSON_AddNumberToObject(item,key_stepperid, i);
	cJSON_AddNumberToObject(item,key_position, data[i]->currentPosition);
	cJSON_AddNumberToObject(item,"isDone", true);
	arraypos++;

	if (moduleController.get(AvailableModules::wifi) != nullptr)
	{
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->sendJsonWebSocketMsg(root);
	}
	// print result - will that work in the case of an xTask?
	Serial.println("++");
	char* s = cJSON_Print(root);
	Serial.println(s);
	free(s);
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
