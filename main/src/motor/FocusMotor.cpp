#include "../../config.h"
#include "FocusMotor.h"

namespace RestApi
{
	void FocusMotor_act()
	{
		deserialize();
		moduleController.get(AvailableModules::motor)->act();
		serialize();
	}

	void FocusMotor_get()
	{
		deserialize();
		moduleController.get(AvailableModules::motor)->get();
		serialize();
	}

	void FocusMotor_set()
	{
		deserialize();
		moduleController.get(AvailableModules::motor)->set();
		serialize();
	}
}

FocusMotor::FocusMotor() : Module() { log_i("ctor"); }
FocusMotor::~FocusMotor() { log_i("~ctor"); }

void FocusMotor::act()
{
	if (DEBUG)
		Serial.println("motor_act_fct");
	DynamicJsonDocument *j = WifiController::getJDoc();
	if (j->containsKey(key_motor))
	{
		if ((*j)[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < (*j)[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>((*j)[key_motor][key_steppers][i][key_stepperid]);

				if ((*j)[key_motor][key_steppers][i].containsKey(key_speed))
					data[s]->speed = (*j)[key_motor][key_steppers][i][key_speed];

				if ((*j)[key_motor][key_steppers][i].containsKey(key_position))
					data[s]->targetPosition = (*j)[key_motor][key_steppers][i][key_position];

				if ((*j)[key_motor][key_steppers][i].containsKey(key_isforever))
					data[s]->isforever = (*j)[key_motor][key_steppers][i][key_isforever];

				if ((*j)[key_motor][key_steppers][i].containsKey(key_isabs))
					data[s]->absolutePosition = (*j)[key_motor][key_steppers][i][key_isabs];

				if ((*j)[key_motor][key_steppers][i].containsKey(key_isaccel))
					data[s]->isaccelerated = (*j)[key_motor][key_steppers][i][key_isaccel];

				if ((*j)[key_motor][key_steppers][i].containsKey(key_isstop))
					stopStepper(s);
				else
				{
#if defined IS_PS3 || defined IS_PS4
					if (ps_c.IS_PSCONTROLER_ACTIVE)
						ps_c.IS_PSCONTROLER_ACTIVE = false; // override PS controller settings #TODO: Somehow reset it later?
#endif
					startStepper(s);
				}
			}
		}
	}
	WifiController::getJDoc()->clear();
}

void FocusMotor::startStepper(int i)
{
	log_i("start stepper:%i isforver:%i", i, data[i]->isforever);
	if (!steppers[i]->areOutputsEnabled())
		steppers[i]->enableOutputs();
	if (!data[i]->isforever)
	{
		if (data[i]->absolutePosition)
		{
			// absolute position coordinates
			steppers[i]->moveTo(data[i]->targetPosition);
			steppers[i]->run();
		}
		else
		{
			// relative position coordinates
			steppers[i]->move(data[i]->targetPosition);
			steppers[i]->run();
		}
	}
	else if (data[i]->isforever)
	{
		if (data[i]->speed == 0)
			stopStepper(i);
		else
		{
			steppers[i]->setMaxSpeed(data[i]->maxspeed);
			steppers[i]->setSpeed(data[i]->speed);
			steppers[i]->runSpeed();
		}
	}
	data[i]->currentPosition = steppers[i]->currentPosition();
}

void FocusMotor::set()
{
	DynamicJsonDocument *doc = WifiController::getJDoc();
	if (doc->containsKey(key_motor))
	{
		if ((*doc)[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < (*doc)[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>((*doc)[key_motor][key_steppers][i][key_stepperid]);
				pins[i]->DIR = (*doc)[key_motor][key_steppers][i][key_dir];
				pins[i]->STEP = (*doc)[key_motor][key_steppers][i][key_step];
				pins[i]->ENABLE = (*doc)[key_motor][key_steppers][i][key_enable];
				pins[i]->direction_inverted = (*doc)[key_motor][key_steppers][i][key_dir_inverted];
				pins[i]->step_inverted = (*doc)[key_motor][key_steppers][i][key_step_inverted];
				pins[i]->enable_inverted = (*doc)[key_motor][key_steppers][i][key_enable_inverted];
				if ((*doc)[key_motor][key_steppers][i].containsKey(key_min_position))
					pins[i]->min_position = (*doc)[key_motor][key_steppers][i][key_min_position];
				if ((*doc)[key_motor][key_steppers][i].containsKey(key_max_position))
					pins[i]->max_position = (*doc)[key_motor][key_steppers][i][key_max_position];
			}
			Config::setMotorPinConfig(pins);
			setup();
		}
	}
	doc->clear();
}

void FocusMotor::get()
{
	DynamicJsonDocument *doc = WifiController::getJDoc();
	doc->clear();
	for (int i = 0; i < steppers.size(); i++)
	{
		(*doc)[key_steppers][i][key_stepperid] = i;
		(*doc)[key_steppers][i][key_dir] = pins[i]->DIR;
		(*doc)[key_steppers][i][key_step] = pins[i]->STEP;
		(*doc)[key_steppers][i][key_enable] = pins[i]->ENABLE;
		(*doc)[key_steppers][i][key_dir_inverted] = pins[i]->direction_inverted;
		(*doc)[key_steppers][i][key_step_inverted] = pins[i]->step_inverted;
		(*doc)[key_steppers][i][key_enable_inverted] = pins[i]->enable_inverted;
		(*doc)[key_steppers][i][key_position] = data[i]->currentPosition;
		(*doc)[key_steppers][i][key_speed] = data[i]->speed;
		(*doc)[key_steppers][i][key_speedmax] = data[i]->maxspeed;
		(*doc)[key_steppers][i][key_max_position] = pins[i]->max_position;
		(*doc)[key_steppers][i][key_min_position] = pins[i]->min_position;
		(*doc)[key_steppers][i][key_current_position] = pins[i]->current_position;
	}
}

void FocusMotor::setup()
{
	// get pins from config
	Config::getMotorPins(pins);
	// create the stepper

	for (int i = 0; i < steppers.size(); i++)
	{
		data[i] = new MotorData();
		log_i("Pins: Step: %i Dir: %i Enable:%i", pins[i]->STEP, pins[i]->DIR, pins[i]->ENABLE);
		steppers[i] = new AccelStepper(AccelStepper::DRIVER, pins[i]->STEP, pins[i]->DIR);
		steppers[i]->setEnablePin(pins[i]->ENABLE);
		steppers[i]->setPinsInverted(pins[i]->step_inverted, pins[i]->direction_inverted, pins[i]->enable_inverted);
	}

	/*
	   Motor related settings
	*/
	Serial.println("Setting Up Motors");
	Serial.println("Setting Up Motor A,X,Y,Z");
	for (int i = 0; i < steppers.size(); i++)
	{
		steppers[i]->setMaxSpeed(MAX_VELOCITY_A);
		steppers[i]->setAcceleration(MAX_ACCELERATION_A);
		steppers[i]->enableOutputs();
		steppers[i]->runToNewPosition(-100);
		steppers[i]->runToNewPosition(100);
		steppers[i]->setCurrentPosition(pins[i]->current_position);
		steppers[i]->disableOutputs();
	}
}

void FocusMotor::loop()
{
	for (int i = 0; i < steppers.size(); i++)
	{
		if (steppers[i] != nullptr && pins[i]->DIR > 0)
		{
			if (data[i]->isforever)
			{
				//log_i("forever drive");
				steppers[i]->setSpeed(data[i]->speed);
				steppers[i]->setMaxSpeed(data[i]->maxspeed);
				steppers[i]->runSpeed();
			}
			else
			{
				// run at constant speed
				if (data[i]->isaccelerated)
				{
					steppers[i]->run();
				}
				else
				{
					steppers[i]->runSpeedToPosition();
				}
				// checks if a stepper is still running
				if (steppers[i]->distanceToGo() == 0 && steppers[i]->areOutputsEnabled())
				{
					log_i("stop stepper:%i", i);
					// if not turn it off
					steppers[i]->disableOutputs();
					// send current position to client
					DynamicJsonDocument *jdoc = WifiController::getJDoc();
					jdoc->clear();
					(*jdoc)[key_steppers][i][key_stepperid] = i;
					(*jdoc)[key_steppers][i][key_position] = data[i]->currentPosition;
					WifiController::sendJsonWebSocketMsg();
					if (pins[i]->max_position != 0 || pins[i]->min_position != 0)
					{
						pins[i]->current_position = steppers[i]->currentPosition();
						Config::setMotorPinConfig(pins);
					}
				}
			}
			data[i]->currentPosition = steppers[i]->currentPosition();
#ifdef DEBUG_MOTOR
			if (pins[i]->DIR > 0 && steppers[i]->areOutputsEnabled())
				log_i("current Pos:%i target pos:%i", data[i]->currentPosition, data[i]->targetPosition);
#endif
		}
	}
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
	data[i]->currentPosition = steppers[i]->currentPosition();
	steppers[i]->disableOutputs();
}

void FocusMotor::startAllDrives()
{
	for (int i = 0; i < steppers.size(); i++)
	{
		log_i("is stepper %i null:%s set speed/max %i/%i step:%i dir%i enablepin:%i outputenabled:%s",
			  i,
			  boolToChar(data[i] == nullptr),
			  data[i]->speed,
			  data[i]->maxspeed,
			  pins[i]->STEP,
			  pins[i]->DIR,
			  pins[i]->ENABLE,
			  boolToChar(steppers[i]->areOutputsEnabled()));
		if (!steppers[i]->areOutputsEnabled())
			steppers[i]->enableOutputs();
		steppers[i]->setSpeed(data[i]->speed);
		steppers[i]->setMaxSpeed(data[i]->maxspeed);
		if (!data[i]->isforever)
		{
			if (data[i]->absolutePosition)
			{
				// absolute position coordinates
				steppers[i]->moveTo(data[i]->targetPosition);
				steppers[i]->run();
			}
			else
			{
				// relative position coordinates
				steppers[i]->move(data[i]->targetPosition);
				steppers[i]->run();
			}
		}
		data[i]->currentPosition = steppers[i]->currentPosition();
	}
}
