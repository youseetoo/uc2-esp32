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

	void FocusMotor_set()
	{
		serialize(moduleController.get(AvailableModules::motor)->set(deserialize()));
	}

	void FocusMotor_setCalibration()
	{
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		serialize(motor->setMinMaxRange(deserialize()));
	}
}

FocusMotor::FocusMotor() : Module() { log_i("ctor"); }
FocusMotor::~FocusMotor() { log_i("~ctor"); }

int FocusMotor::act(DynamicJsonDocument doc)
{
	if (DEBUG)
		Serial.println("motor_act_fct");
	if (doc.containsKey(key_motor))
	{
		if (doc[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < doc[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>(doc[key_motor][key_steppers][i][key_stepperid]);

				if (doc[key_motor][key_steppers][i].containsKey(key_speed))
					data[s]->speed = doc[key_motor][key_steppers][i][key_speed];

				if (doc[key_motor][key_steppers][i].containsKey(key_position))
					data[s]->targetPosition = doc[key_motor][key_steppers][i][key_position];

				if (doc[key_motor][key_steppers][i].containsKey(key_isforever))
					data[s]->isforever = doc[key_motor][key_steppers][i][key_isforever];

				if (doc[key_motor][key_steppers][i].containsKey(key_isabs))
					data[s]->absolutePosition = doc[key_motor][key_steppers][i][key_isabs];

				if (doc[key_motor][key_steppers][i].containsKey(key_isaccel))
					data[s]->isaccelerated = doc[key_motor][key_steppers][i][key_isaccel];

				if (doc[key_motor][key_steppers][i].containsKey(key_isstop))
					stopStepper(s);
				else
				{
					if (pins[s]->max_position != 0 || pins[s]->min_position != 0)
					{
						if ((pins[s]->current_position + data[s]->speed / 200 >= pins[s]->max_position && data[s]->speed > 0) || (pins[s]->current_position + data[s]->speed / 200 <= pins[s]->min_position && data[s]->speed < 0))
						{
							return 1;
						}
						else
							startStepper(s);
					}
					else
						startStepper(s);
				}
			}
		}
	}
	return 1;
}

void FocusMotor::startStepper(int i)
{
	log_i("start stepper:%i isforver:%i", i, data[i]->isforever);
	enableEnablePin(i);
	data[i]->stopped = false;
	if (!data[i]->isforever)
	{
		steppers[i]->setSpeed(data[i]->speed);
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
		steppers[i]->setMaxSpeed(data[i]->maxspeed);
		steppers[i]->setSpeed(data[i]->speed);
		steppers[i]->runSpeed();
	}
	pins[i]->current_position = steppers[i]->currentPosition();
}

int FocusMotor::set(DynamicJsonDocument doc)
{
	if (doc.containsKey(key_motor))
	{
		if (doc[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < doc[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>(doc[key_motor][key_steppers][i][key_stepperid]);
				pins[s]->DIR = doc[key_motor][key_steppers][i][key_dir];
				pins[s]->STEP = doc[key_motor][key_steppers][i][key_step];
				pins[s]->ENABLE = doc[key_motor][key_steppers][i][key_enable];
				pins[s]->direction_inverted = doc[key_motor][key_steppers][i][key_dir_inverted];
				pins[s]->step_inverted = doc[key_motor][key_steppers][i][key_step_inverted];
				pins[s]->enable_inverted = doc[key_motor][key_steppers][i][key_enable_inverted];
			}
			Config::setMotorPinConfig(pins);
			setup();
		}
	}
	return 1;
}

int FocusMotor::setMinMaxRange(DynamicJsonDocument  doc)
{
	if (doc.containsKey(key_motor))
	{
		if (doc[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < doc[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>(doc[key_motor][key_steppers][i][key_stepperid]);
				if (doc[key_motor][key_steppers][i].containsKey(key_min_position) && doc[key_motor][key_steppers][i].containsKey(key_max_position))
					resetMotorPos(s);
				else if (doc[key_motor][key_steppers][i].containsKey(key_min_position))
					applyMinPos(s);
				else if (doc[key_motor][key_steppers][i].containsKey(key_max_position))
					applyMaxPos(s);
			}
		}
	}
	return 1;
}

void FocusMotor::resetMotorPos(int i)
{
	pins[i]->min_position = 0;
	pins[i]->max_position = 0;
	Config::setMotorPinConfig(pins);
}

void FocusMotor::applyMinPos(int i)
{
	steppers[i]->setCurrentPosition(0);
	pins[i]->current_position = 0;
	pins[i]->min_position = 0;
	log_i("curPos:%i min_pos:%i", pins[i]->current_position, pins[i]->min_position);
	Config::setMotorPinConfig(pins);
}

void FocusMotor::applyMaxPos(int i)
{
	pins[i]->max_position = steppers[i]->currentPosition();
	log_i("curPos:%i max_pos:%i", pins[i]->current_position, pins[i]->max_position);
	Config::setMotorPinConfig(pins);
}

DynamicJsonDocument FocusMotor::get(DynamicJsonDocument ob)
{
	ob.clear();
	for (int i = 0; i < steppers.size(); i++)
	{
		ob[key_steppers][i][key_stepperid] = i;
		ob[key_steppers][i][key_dir] = pins[i]->DIR;
		ob[key_steppers][i][key_step] = pins[i]->STEP;
		ob[key_steppers][i][key_enable] = pins[i]->ENABLE;
		ob[key_steppers][i][key_dir_inverted] = pins[i]->direction_inverted;
		ob[key_steppers][i][key_step_inverted] = pins[i]->step_inverted;
		ob[key_steppers][i][key_enable_inverted] = pins[i]->enable_inverted;
		ob[key_steppers][i][key_speed] = data[i]->speed;
		ob[key_steppers][i][key_speedmax] = data[i]->maxspeed;
		ob[key_steppers][i][key_max_position] = pins[i]->max_position;
		ob[key_steppers][i][key_min_position] = pins[i]->min_position;
		ob[key_steppers][i][key_position] = pins[i]->current_position;
	}
	return ob;
}

void FocusMotor::setup()
{
	// get pins from config
	Config::getMotorPins(pins);
	// create the stepper
	isShareEnable = shareEnablePin();
	for (int i = 0; i < steppers.size(); i++)
	{
		data[i] = new MotorData();
		log_i("Pins: Step: %i Dir: %i Enable:%i min_pos:%i max_pos:%i", pins[i]->STEP, pins[i]->DIR, pins[i]->ENABLE, pins[i]->min_position, pins[i]->max_position);
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
	int arraypos = 0;
	for (int i = 0; i < steppers.size(); i++)
	{
		if (steppers[i] != nullptr && pins[i]->DIR > 0)
		{
			if (pins[i]->max_position != 0 || pins[i]->min_position != 0)
			{
				if ((pins[i]->current_position + data[i]->speed / 200 >= pins[i]->max_position && data[i]->speed > 0) || (pins[i]->current_position + data[i]->speed / 200 <= pins[i]->min_position && data[i]->speed < 0))
				{
					stopStepper(i);
					sendMotorPos(i, arraypos);
					return;
				}
			}
			steppers[i]->setSpeed(data[i]->speed);
			steppers[i]->setMaxSpeed(data[i]->maxspeed);
			if (data[i]->isforever)
			{
				steppers[i]->runSpeed();
			}
			else
			{
				// run at constant speed

				if (data[i]->absolutePosition)
				{
					// absolute position coordinates
					steppers[i]->moveTo(data[i]->targetPosition);
				}
				else
				{
					// relative position coordinates
					steppers[i]->move(data[i]->targetPosition);
				}
				// checks if a stepper is still running
				if (steppers[i]->distanceToGo() == 0 && !data[i]->stopped)
				{
					log_i("stop stepper:%i", i);
					// if not turn it off
					stopStepper(i);
					sendMotorPos(i, arraypos);
					if (pins[i]->max_position != 0 || pins[i]->min_position != 0)
					{
						pins[i]->current_position = steppers[i]->currentPosition();
						Config::setMotorPinConfig(pins);
					}
				}
			}
			// send current position to client
			//{"steppers":[{"stepperid":1,"position":3}]}

#ifdef DEBUG_MOTOR
			if (pins[i]->DIR > 0 && steppers[i]->areOutputsEnabled())
				log_i("current Pos:%i target pos:%i", pins[i]->current_position, data[i]->targetPosition);
#endif
		}
	}

	arraypos = 0;
	if (millis() >= nextSocketUpdateTime)
	{
		for (int i = 0; i < steppers.size(); i++)
		{
			if (!data[i]->stopped)
			{
				pins[i]->current_position = steppers[i]->currentPosition();
				sendMotorPos(i, arraypos);
			}
		}
		nextSocketUpdateTime = millis() + 500UL;
	}

	if (isShareEnable)
	{
		disableEnablePin(-1);
	}
}

void FocusMotor::sendMotorPos(int i, int arraypos)
{
	DynamicJsonDocument doc(4096);
	doc[key_steppers][arraypos][key_stepperid] = i;
	doc[key_steppers][arraypos][key_position] = pins[i]->current_position;
	arraypos++;
	WifiController::sendJsonWebSocketMsg(doc);
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
	pins[i]->current_position = steppers[i]->currentPosition();
	data[i]->stopped = true;
	disableEnablePin(i);
}

void FocusMotor::startAllDrives()
{
	for (int i = 0; i < steppers.size(); i++)
	{
		startStepper(i);
	}
}

bool FocusMotor::shareEnablePin()
{
	bool share = false;
	int lastval = 0;
	for (int i = 0; i < 4; i++)
	{
		if (pins[i]->ENABLE > 0 && lastval == pins[i]->ENABLE)
		{
			share = true;
		}
		else if (pins[i]->ENABLE > 0)
			lastval = pins[i]->ENABLE;
	}
	log_i("motors share same enable pin:%s", share);
	return share;
}

void FocusMotor::disableEnablePin(int i)
{
	if (!isShareEnable && i > -1)
		steppers[i]->disableOutputs();
	else
	{
		if (data[Stepper::A]->stopped && data[Stepper::X]->stopped &&
			data[Stepper::Y]->stopped &&
			data[Stepper::Z]->stopped &&
			(steppers[Stepper::A]->areOutputsEnabled() || steppers[Stepper::X]->areOutputsEnabled() || steppers[Stepper::Y]->areOutputsEnabled() || steppers[Stepper::Z]->areOutputsEnabled()))
		{
			log_i("disable motors A enable:%i stop:%i X enable:%i stop:%i Y enable:%i stop:%i Z enable:%i stop:%i",
				  steppers[Stepper::A]->areOutputsEnabled(),
				  data[Stepper::A]->stopped,
				  steppers[Stepper::X]->areOutputsEnabled(),
				  data[Stepper::X]->stopped,
				  steppers[Stepper::Y]->areOutputsEnabled(),
				  data[Stepper::Y]->stopped,
				  steppers[Stepper::Z]->areOutputsEnabled(),
				  data[Stepper::Z]->stopped);

			if (pins[Stepper::A]->ENABLE > 0 && steppers[Stepper::A]->areOutputsEnabled())
				steppers[Stepper::A]->disableOutputs();
			else if (pins[Stepper::X]->ENABLE > 0 && steppers[Stepper::X]->areOutputsEnabled())
				steppers[Stepper::X]->disableOutputs();
			else if (pins[Stepper::Y]->ENABLE > 0 && steppers[Stepper::Y]->areOutputsEnabled())
				steppers[Stepper::Y]->disableOutputs();
			else if (pins[Stepper::Z]->ENABLE > 0 && steppers[Stepper::Z]->areOutputsEnabled())
				steppers[Stepper::Z]->disableOutputs();
		}
	}
}

void FocusMotor::enableEnablePin(int i)
{
	if (isShareEnable)
	{
		bool enable = false;
		for (size_t i = 0; i < steppers.size(); i++)
		{
			if (steppers[i]->areOutputsEnabled() && !enable)
				enable = true;
		}

		if (!enable)
		{
			for (size_t i = 0; i < steppers.size(); i++)
			{
				if (pins[i]->ENABLE > 0 && !enable)
				{
					steppers[i]->enableOutputs();
					enable = true;
					return;
				}
			}
		}
	}
	else if (!steppers[i]->areOutputsEnabled())
		steppers[i]->enableOutputs();
}
