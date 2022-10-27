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

	void FocusMotor_setCalibration()
	{
		deserialize();
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		motor->setMinMaxRange();
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

				if ((*j)[key_motor][key_steppers][i][key_isstop])
					stopStepper(s);
				else
				{
#if defined IS_PS3 || defined IS_PS4
					// the Serial/Wifi has always priority / PS controller is slave
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
	log_i("start stepper:%i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i", i, data[i]->isforever, data[i]->speed, data[i]->maxspeed, data[i]->targetPosition, data[i]->absolutePosition);

	// set speed
	steppers[i]->setMaxSpeed(data[i]->maxspeed);
	steppers[i]->setSpeed(data[i]->speed);

	// Handle the Enable pin that is either same for all or each motor has its own
	if (isShareEnable)
	{
		bool enable = false;
		for (size_t i = 0; i < steppers.size(); i++)
		{
			if (steppers[i]->areOutputsEnabled())
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
	else if (data[i]->isforever) // initiate motion -> afterwards steps will be performed in .loop()
	{
		if (data[i]->speed == 0)
			stopStepper(i);
		else
		{
			steppers[i]->runSpeed();
		}
	}
	else
	{ // drive a defined distance
		// absolute
		if (data[i]->absolutePosition)
		{
			// initiate a move to an absolute position
			steppers[i]->moveTo(data[i]->targetPosition);
		}
		// relative
		else
		{
			// initiate a move to a relative position
			steppers[i]->move(data[i]->targetPosition);
		}
	}
	pins[i]->current_position = steppers[i]->currentPosition();
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
				pins[s]->DIR = (*doc)[key_motor][key_steppers][i][key_dir];
				pins[s]->STEP = (*doc)[key_motor][key_steppers][i][key_step];
				pins[s]->ENABLE = (*doc)[key_motor][key_steppers][i][key_enable];
				pins[s]->direction_inverted = (*doc)[key_motor][key_steppers][i][key_dir_inverted];
				pins[s]->step_inverted = (*doc)[key_motor][key_steppers][i][key_step_inverted];
				pins[s]->enable_inverted = (*doc)[key_motor][key_steppers][i][key_enable_inverted];

				// applying "hard" boundaries for step-range
				pins[s]->min_position = (*doc)[key_motor][key_steppers][i][key_min_position];
				pins[s]->max_position = (*doc)[key_motor][key_steppers][i][key_max_position];
			}
			Config::setMotorPinConfig(pins);
			setup();
		}
	}
	doc->clear();
}

void FocusMotor::setMinMaxRange()
{
	DynamicJsonDocument *doc = WifiController::getJDoc();
	if (doc->containsKey(key_motor))
	{
		if ((*doc)[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < (*doc)[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>((*doc)[key_motor][key_steppers][i][key_stepperid]);
				if ((*doc)[key_motor][key_steppers][i].containsKey(key_min_position) && (*doc)[key_motor][key_steppers][i].containsKey(key_max_position))
					resetMotorPos(s);
				else if ((*doc)[key_motor][key_steppers][i].containsKey(key_min_position))
					applyMinPos(s);
				else if ((*doc)[key_motor][key_steppers][i].containsKey(key_max_position))
					applyMaxPos(s);
			}
		}
	}
}

void FocusMotor::resetMotorPos(int i)
{
	/*
	pins[i]->min_position = 0;
	pins[i]->max_position = 0;
	Config::setMotorPinConfig(pins);
	*/
}

void FocusMotor::applyMinPos(int i)
{
	/*
	steppers[i]->setCurrentPosition(0);
	pins[i]->current_position = 0;
	pins[i]->min_position = 0;
	log_i("curPos:%i min_pos:%i", pins[i]->current_position, pins[i]->min_position);
	Config::setMotorPinConfig(pins);
	*/
}

void FocusMotor::applyMaxPos(int i)
{
	/*
	pins[i]->max_position = steppers[i]->currentPosition();
	log_i("curPos:%i max_pos:%i", pins[i]->current_position, pins[i]->max_position);
	Config::setMotorPinConfig(pins);
	*/
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
		(*doc)[key_steppers][i][key_speed] = data[i]->speed;
		(*doc)[key_steppers][i][key_speedmax] = data[i]->maxspeed;
		(*doc)[key_steppers][i][key_max_position] = pins[i]->max_position;
		(*doc)[key_steppers][i][key_min_position] = pins[i]->min_position;
		(*doc)[key_steppers][i][key_position] = pins[i]->current_position;
	}
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
		// we have only one enable pin for all - in most cases it's inverted
		pinMode(pins[i]->ENABLE, OUTPUT);
		digitalWrite(pins[i]->ENABLE, LOW);
		// steppers[i]->setEnablePin(pins[i]->ENABLE);
		// steppers[i]->setPinsInverted(pins[i]->step_inverted, pins[i]->direction_inverted, pins[i]->enable_inverted);
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

	// will be called in every global CMU cycle
	int arraypos = 0; // TODO: When is this value changed?

	// iterate over all available motors
	for (int i = 0; i < steppers.size(); i++)
	{
		// move motor only if available
		if (steppers[i] != nullptr && pins[i]->DIR > 0)
		{
			// set speed
			steppers[i]->setSpeed(data[i]->speed);
			steppers[i]->setMaxSpeed(data[i]->maxspeed);
			// turn motors forever if necessary
			if (data[i]->isforever)
			{
				steppers[i]->runSpeed();
			}
			else
			{
				// run at constant speed
				if (data[i]->isaccelerated) // run with acceleration
				{
					steppers[i]->run();
				}
				else // run at constant speed
				{
					steppers[i]->runSpeedToPosition();
				}
				// checks if a stepper is still running
				if (steppers[i]->distanceToGo() == 0 && !data[i]->stopped)
				{
					log_i("stop stepper:%i", i);
					// if not turn it off
					if (!isShareEnable)
						steppers[i]->disableOutputs();
					data[i]->stopped = true;
					sendMotorPos(i, arraypos);

					// checks if a stepper is still running
					if (steppers[i]->distanceToGo() == 0) // TODO Need to check this here? : && steppers[i]->areOutputsEnabled())
					{
#ifdef DEBUG_MOTOR
						log_i("stop stepper:%i", i);
#endif
					}

#ifdef DEBUG_MOTOR
					if (pins[i]->DIR > 0 && steppers[i]->areOutputsEnabled())
						log_i("current Pos:%i target pos:%i", pins[i]->current_position, data[i]->targetPosition);
#endif
				}
			}
		}
	}
	if (isShareEnable)
	{
		// check if any motor is running - if not disable the enable pin // TODO: This is as intended; Enable should stay "on" in some cases since you will loos the position of the motor (e.g. on/off microsteppeing, etc.)
		if (data[Stepper::A]->stopped && data[Stepper::X]->stopped && data[Stepper::Y]->stopped && data[Stepper::Z]->stopped)
		{
			if (pins[Stepper::A]->ENABLE > 0)
				steppers[Stepper::A]->disableOutputs();
			else if (pins[Stepper::X]->ENABLE > 0)
				steppers[Stepper::X]->disableOutputs();
			else if (pins[Stepper::Y]->ENABLE > 0)
				steppers[Stepper::Y]->disableOutputs();
			else if (pins[Stepper::Z]->ENABLE > 0)
				steppers[Stepper::Z]->disableOutputs();
		}
	}
}

void FocusMotor::sendMotorPos(int i, int arraypos)
{
	DynamicJsonDocument *jdoc = WifiController::getJDoc();
	jdoc->clear();
	(*jdoc)[key_steppers][arraypos][key_stepperid] = i;
	(*jdoc)[key_steppers][arraypos][key_position] = pins[i]->current_position;
	arraypos++;
	WifiController::sendJsonWebSocketMsg();
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
	if (!isShareEnable)
		steppers[i]->disableOutputs();
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
	return share;
}
