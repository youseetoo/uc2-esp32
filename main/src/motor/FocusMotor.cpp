#include "../../config.h"
#include "FocusMotor.h"
#include "../../pindef.h"
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
				Stepper s = static_cast<Stepper>((*j)[key_motor][key_steppers][i][key_steppinperid]); // enumerate the stepper 0,1,2,3 => A,X,Y,Z

				// if no speed is given, use the default speed
				if ((*j)[key_motor][key_steppers][i].containsKey(key_speed))
					data[s]->speed = (*j)[key_motor][key_steppers][i][key_speed];
				else
					data[s]->speed = 0;

				if ((*j)[key_motor][key_steppers][i].containsKey(key_position))
					data[s]->targetPosition = (*j)[key_motor][key_steppers][i][key_position];

				if ((*j)[key_motor][key_steppers][i].containsKey(key_isforever))
					data[s]->isforever = (*j)[key_motor][key_steppers][i][key_isforever];
				else // if not set, set to false
					data[s]->isforever = false;

				if ((*j)[key_motor][key_steppers][i].containsKey(key_isabs))
					data[s]->absolutePosition = (*j)[key_motor][key_steppers][i][key_isabs];
				else // we always set absolute position to false if not set
					data[s]->absolutePosition = false;

				if ((*j)[key_motor][key_steppers][i].containsKey(key_isaccel))
					data[s]->isaccelerated = (*j)[key_motor][key_steppers][i][key_isaccel];
				else // we always switch off acceleration if not set
					data[s]->isaccelerated = false;

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
		WifiController::getJDoc()->clear();
		// have some return value, that the function was called correctly
		(*j)[key_return] = 1;
	}
	else
	{
		WifiController::getJDoc()->clear();
		(*j)[key_return] = 0;
	}
}

void FocusMotor::startStepper(int i)
{
	log_i("start stepper:%i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i", i, data[i]->isforever, data[i]->speed, data[i]->maxspeed, data[i]->targetPosition, data[i]->absolutePosition);

	// set speed
	steppers[i]->setMaxSpeed(data[i]->maxspeed);
	steppers[i]->setSpeed(data[i]->speed);

	// set position
	data[i]->stopped = false;
	if (data[i]->speed == 0)
	{
		stopStepper(i);
	}
	else
	{
		// FOREVER
		if (data[i]->isforever) // initiate motion -> afterwards steps will be performed in .loop()
		{
			steppers[i]->runSpeed();
		}
		// DEFINED POSITION
		else
		{
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
				Stepper s = static_cast<Stepper>((*doc)[key_motor][key_steppers][i][key_steppinperid]);
				if ((*doc)[key_motor][key_steppers][i].containsKey(key_dirpin))
					pins[s]->DIR = (*doc)[key_motor][key_steppers][i][key_dirpin];

				if ((*doc)[key_motor][key_steppers][i].containsKey(key_steppin))
					pins[s]->STEP = (*doc)[key_motor][key_steppers][i][key_steppin];

				if ((*doc)[key_motor][key_steppers][i].containsKey(key_enablepin))
					pins[s]->ENABLE = (*doc)[key_motor][key_steppers][i][key_enablepin];

				// directly set the pins
				if ((*doc)[key_motor][key_steppers][i].containsKey(key_isen))
					digitalWrite(pins[s]->ENABLE, !(bool)(*doc)[key_motor][key_steppers][i][key_isen]);

				if ((*doc)[key_motor][key_steppers][i].containsKey(key_dirpin_inverted))
					pins[s]->direction_inverted = (*doc)[key_motor][key_steppers][i][key_dirpin_inverted];

				if ((*doc)[key_motor][key_steppers][i].containsKey(key_steppin_inverted))
					pins[s]->step_inverted = (*doc)[key_motor][key_steppers][i][key_steppin_inverted];

				if ((*doc)[key_motor][key_steppers][i].containsKey(key_enablepin_inverted))
					pins[s]->enable_inverted = (*doc)[key_motor][key_steppers][i][key_enablepin_inverted];

				// applying "hard" boundaries for step-range
				if ((*doc)[key_motor][key_steppers][i].containsKey(key_min_position))
					pins[s]->min_position = (*doc)[key_motor][key_steppers][i][key_min_position];

				if ((*doc)[key_motor][key_steppers][i].containsKey(key_max_position))
					pins[s]->max_position = (*doc)[key_motor][key_steppers][i][key_max_position];

				// override current position
				if ((*doc)[key_motor][key_steppers][i].containsKey(key_position))
					pins[s]->current_position = (*doc)[key_motor][key_steppers][i][key_position];

				// adjust accelaration
				/* //TODO: NOT IMPLEMENTED
				if ((*doc)[key_motor][key_steppers][i].containsKey(key_acceleration))
					pins[s]->acceleration = (*doc)[key_motor][key_steppers][i][key_acceleration];
				*/
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
				Stepper s = static_cast<Stepper>((*doc)[key_motor][key_steppers][i][key_steppinperid]);
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
{ //{"task":"/motor_get", "position":1}
	DynamicJsonDocument *doc = WifiController::getJDoc();
	// only return position if necessary, else return everything
	if (doc->containsKey(key_position))
	{
		doc->clear();
		for (int i = 0; i < steppers.size(); i++)
		{
			// update position and push it to the json
			pins[i]->current_position = steppers[i]->currentPosition();
			(*doc)[key_motor][key_steppers][i][key_steppinperid] = i;
			(*doc)[key_motor][key_steppers][i][key_position] = pins[i]->current_position;
		}
		return;
	}

	// only return if motor is still busy
	if (doc->containsKey(key_stopped))
	{
		doc->clear();
		for (int i = 0; i < steppers.size(); i++)
		{
			// update position and push it to the json
			(*doc)[key_motor][key_steppers][i][key_stopped] = !data[i]->stopped;
		}
		return;
	}

	// return all information
	doc->clear();
	for (int i = 0; i < steppers.size(); i++)
	{
		(*doc)[key_steppers][i][key_steppinperid] = i;
		(*doc)[key_steppers][i][key_dirpin] = pins[i]->DIR;
		(*doc)[key_steppers][i][key_steppin] = pins[i]->STEP;
		(*doc)[key_steppers][i][key_enablepin] = pins[i]->ENABLE;
		(*doc)[key_steppers][i][key_dirpin_inverted] = pins[i]->direction_inverted;
		(*doc)[key_steppers][i][key_steppin_inverted] = pins[i]->step_inverted;
		(*doc)[key_steppers][i][key_enablepin_inverted] = pins[i]->enable_inverted;
		(*doc)[key_steppers][i][key_speed] = data[i]->speed;
		(*doc)[key_steppers][i][key_speedmax] = data[i]->maxspeed;
		(*doc)[key_steppers][i][key_max_position] = pins[i]->max_position;
		(*doc)[key_steppers][i][key_min_position] = pins[i]->min_position;
		(*doc)[key_steppers][i][key_position] = pins[i]->current_position;
		(*doc)[key_steppers][i][key_stopped] = !data[i]->stopped;
	}
}

void FocusMotor::setup()
{
	// get pins from config
	Config::getMotorPins(pins);

	// if pins have not been set => load defaults
	if(not pins[0]->STEP) pins[0]->STEP=PIN_DEF_MOTOR_STP_A;
	if(not pins[0]->DIR) pins[0]->DIR=PIN_DEF_MOTOR_DIR_A;
	if(not pins[0]->ENABLE) pins[0]->ENABLE=PIN_DEF_MOTOR_EN_A;
	if(not pins[1]->STEP) pins[1]->STEP=PIN_DEF_MOTOR_STP_X;
	if(not pins[1]->DIR) pins[1]->DIR=PIN_DEF_MOTOR_DIR_X;
	if(not pins[1]->ENABLE) pins[1]->ENABLE=PIN_DEF_MOTOR_EN_X;
	if(not pins[2]->STEP) pins[2]->STEP=PIN_DEF_MOTOR_STP_Y;
	if(not pins[2]->DIR) pins[2]->DIR=PIN_DEF_MOTOR_DIR_Y;
	if(not pins[2]->ENABLE) pins[2]->ENABLE=PIN_DEF_MOTOR_EN_Y;
	if(not pins[3]->STEP) pins[3]->STEP=PIN_DEF_MOTOR_STP_Z;
	if(not pins[3]->DIR) pins[3]->DIR=PIN_DEF_MOTOR_DIR_Z;
	if(not pins[3]->ENABLE) pins[3]->ENABLE=PIN_DEF_MOTOR_EN_Z;

	// write updated motor config to flash
	Config::setMotorPinConfig(pins);
	
	// create the stepper
	// isShareEnable = shareEnablePin();
	for (int i = 0; i < steppers.size(); i++)
	{
		data[i] = new MotorData();
		data[i]->stopped = false; // inidcate that we are busy now to keep serial happy
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
		// steppers[i]->enableOutputs();
		// steppers[i]->runToNewPosition(-100);
		// steppers[i]->runToNewPosition(100);
		steppers[i]->setCurrentPosition(pins[i]->current_position);
		steppers[i]->disableOutputs();
		data[i]->stopped = true; // inidcate that we are busy now to keep serial happy
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
		if (steppers[i] != nullptr && pins[i]->DIR > 0) // TODO: Makes sense, but if no config is given, the return value fails.
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
			{ // only run if not already at position
				if (steppers[i]->distanceToGo() != 0)
				{
					data[i]->stopped=false;

					// run at constant speed
					if (data[i]->isaccelerated) // run with acceleration
					{
						steppers[i]->run();
					}
					else // run at constant speed
					{
						steppers[i]->runSpeedToPosition();
					}
				}
				else
				{
					if (!data[i]->stopped){
						// send message that motor is done only once to not slow down the loop
						// printf("{'m':%1i, 'isBusy':0}", i);
						Serial.print("{'motor':");Serial.print(i);Serial.print(", 'isDone':"); Serial.print(!data[i]->stopped); Serial.println("}");
					}
					data[i]->stopped=true;
				}
			}
		}
	}
}

void FocusMotor::sendMotorPos(int i, int arraypos)
{
	DynamicJsonDocument *jdoc = WifiController::getJDoc();
	jdoc->clear();
	(*jdoc)[key_steppers][arraypos][key_steppinperid] = i;
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
	// if (!isShareEnable)
	//	steppers[i]->disableOutputs();
}

void FocusMotor::startAllDrives()
{
	for (int i = 0; i < steppers.size(); i++)
	{
		startStepper(i);
	}
}

bool FocusMotor::shareEnablePin()
{ /*
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
	 share = true; // TODO: special case
	 return share;

	 */
}
