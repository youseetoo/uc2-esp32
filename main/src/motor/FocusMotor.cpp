#include "FocusMotor.h"
#include "../../pindef.h"
#include "../serial/SerialProcess.h"


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
		{ // enumerate the stepper 0,1,2,3 => A,X,Y,Z
			for (int i = 0; i < doc[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>(doc[key_motor][key_steppers][i][key_stepperid]);

				if (doc[key_motor][key_steppers][i].containsKey(key_speed))
					data[s]->speed = doc[key_motor][key_steppers][i][key_speed];
				else
					data[s]->speed = 0;

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
					data[s]->acceleration = 20000;


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
				
				log_i("start stepper (act): motor:%i, index: %i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i, isacceleration: %i", s, i, data[s]->isforever, data[s]->speed, data[s]->maxspeed, data[s]->targetPosition, data[s]->absolutePosition, data[s]->isaccelerated);

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

	// enableEnablePin(i);
	data[i]->stopped = false;

	// set motor state according to commands
	// 1; Move certain steps
	if (!data[i]->isforever)
	{
		steppers[i]->setSpeed(data[i]->speed);
		// 1.1; Move to absolute position
		if (data[i]->absolutePosition)
		{
			// absolute position coordinates
			steppers[i]->moveTo(data[i]->targetPosition);
		}
		// 1.2; Move to relative position
		else
		{
			// relative position coordinates
			steppers[i]->move(data[i]->targetPosition);
		}

		// perform first step
		if (data[i]->isaccelerated)
		{
			steppers[i]->setAcceleration(data[i]->acceleration);
			steppers[i]->run();
		}
		else
		{
			steppers[i]->runSpeedToPosition();
		}
	}
	// 2; Move forever
	else
	{
		steppers[i]->setMaxSpeed(data[i]->maxspeed);
		steppers[i]->setSpeed(data[i]->speed);
		steppers[i]->runSpeed();
	}
	// log_i("start stepper:%i isforver:%i, speed: %L, maxSpeed: %L, steps: %i, isabsolute: %i, isacceleration: %i", i, data[i]->isforever, steppers[i]->speed(), steppers[i]->maxSpeed(), data[i]->targetPosition, data[i]->absolutePosition, data[i]->isaccelerated);
	Serial.println("Speed (motor/data)" + String(steppers[i]->speed()) + " / " + String(data[i]->speed));
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
				pins[s]->DIR = doc[key_motor][key_steppers][i][key_dirpin];
				pins[s]->STEP = doc[key_motor][key_steppers][i][key_steppin];
				pins[s]->ENABLE = doc[key_motor][key_steppers][i][key_enablepin];
				pins[s]->direction_inverted = doc[key_motor][key_steppers][i][key_dirpin_inverted];
				pins[s]->step_inverted = doc[key_motor][key_steppers][i][key_steppin_inverted];
				pins[s]->enable_inverted = doc[key_motor][key_steppers][i][key_enablepin_inverted];
				pins[s]->current_position = doc[key_motor][key_steppers][i][key_position];
				pins[s]->max_position = doc[key_motor][key_steppers][i][key_max_position];
				pins[s]->min_position = doc[key_motor][key_steppers][i][key_min_position];
			}
			Config::setMotorPinConfig(pins);
			setup();
		}
	}
	return 1;
}

int FocusMotor::setMinMaxRange(DynamicJsonDocument doc)
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

DynamicJsonDocument FocusMotor::get(DynamicJsonDocument docin)
{
	log_i("get motor");
	StaticJsonDocument<512> doc; // create return doc
	// only return the position of the stepper
	if (docin.containsKey(key_position))
	{
		docin.clear();
		for (int i = 0; i < steppers.size(); i++)
		{
			// update position and push it to the json
			pins[i]->current_position = steppers[i]->currentPosition();
			doc[key_motor][key_steppers][i][key_stepperid] = i;
			doc[key_motor][key_steppers][i][key_position] = pins[i]->current_position;
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
		doc[key_steppers][i][key_dirpin] = pins[i]->DIR;
		doc[key_steppers][i][key_steppin] = pins[i]->STEP;
		doc[key_steppers][i][key_enablepin] = pins[i]->ENABLE;
		doc[key_steppers][i][key_dirpin_inverted] = pins[i]->direction_inverted;
		doc[key_steppers][i][key_steppin_inverted] = pins[i]->step_inverted;
		doc[key_steppers][i][key_enablepin_inverted] = pins[i]->enable_inverted;
		doc[key_steppers][i][key_speed] = data[i]->speed;
		doc[key_steppers][i][key_speedmax] = data[i]->maxspeed;
		doc[key_steppers][i][key_max_position] = pins[i]->max_position;
		doc[key_steppers][i][key_min_position] = pins[i]->min_position;
		doc[key_steppers][i][key_position] = pins[i]->current_position;
	}

	return doc;
}

void FocusMotor::setup()
{
	// get pins from config
	Config::getMotorPins(pins);

	// if pins have not been set => load defaults

	// MOTOR A
	if (not pins[0]->STEP)
		pins[0]->STEP = PIN_DEF_MOTOR_STP_A;
	if (not pins[0]->DIR)
		pins[0]->DIR = PIN_DEF_MOTOR_DIR_A;
	if (not pins[0]->ENABLE)
		pins[0]->ENABLE = PIN_DEF_MOTOR_EN_A;
	if (pins[0]->enable_inverted == 0)
		pins[0]->enable_inverted = PIN_DEF_MOTOR_EN_A_INVERTED;

	// MOTOR X
	if (not pins[1]->STEP)
		pins[1]->STEP = PIN_DEF_MOTOR_STP_X;
	if (not pins[1]->DIR)
		pins[1]->DIR = PIN_DEF_MOTOR_DIR_X;
	if (not pins[1]->ENABLE)
		pins[1]->ENABLE = PIN_DEF_MOTOR_EN_X;
	if (pins[1]->enable_inverted == 0)
		pins[1]->enable_inverted = PIN_DEF_MOTOR_EN_X_INVERTED;

	// MOTOR Y
	if (not pins[2]->STEP)
		pins[2]->STEP = PIN_DEF_MOTOR_STP_Y;
	if (not pins[2]->DIR)
		pins[2]->DIR = PIN_DEF_MOTOR_DIR_Y;
	if (not pins[2]->ENABLE)
		pins[2]->ENABLE = PIN_DEF_MOTOR_EN_Y;
	if (pins[2]->enable_inverted == 0)
		pins[2]->enable_inverted = PIN_DEF_MOTOR_EN_Y_INVERTED;

	// MOTOR Z
	if (not pins[3]->STEP)
		pins[3]->STEP = PIN_DEF_MOTOR_STP_Z;
	if (not pins[3]->DIR)
		pins[3]->DIR = PIN_DEF_MOTOR_DIR_Z;
	if (not pins[3]->ENABLE)
		pins[3]->ENABLE = PIN_DEF_MOTOR_EN_Z;
	if (pins[3]->enable_inverted == 0)
		pins[3]->enable_inverted = PIN_DEF_MOTOR_EN_Z_INVERTED;

	// write updated motor config to flash
	Config::setMotorPinConfig(pins);
	// create the stepper
	// isShareEnable = shareEnablePin();
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
		// steppers[i]->enableOutputs();
		steppers[i]->runToNewPosition(-100);
		steppers[i]->runToNewPosition(100);
		steppers[i]->setCurrentPosition(pins[i]->current_position);
		steppers[i]->enableOutputs();
		// steppers[i]->disableOutputs();
	}
}

bool FocusMotor::checkIfMinMaxPosIsReached(int arraypos, int i)
{
	if (pins[i]->max_position != 0 || pins[i]->min_position != 0)
	{
		if ((pins[i]->current_position + data[i]->speed / 200 >= pins[i]->max_position && data[i]->speed > 0) || (pins[i]->current_position + data[i]->speed / 200 <= pins[i]->min_position && data[i]->speed < 0))
		{
			stopStepper(i);
			sendMotorPos(i, arraypos);
			return true;
		}
	}
	return false;
}

void FocusMotor::loop()
{
	int arraypos = 0;

	// iterate over all available motors
	for (int i = 0; i < steppers.size(); i++)
	{

		// move motor only if available
		if (steppers[i] != nullptr && pins[i]->DIR > 0)
		{
			// log_i("start stepper:%i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i, isacceleration: %i", i, data[i]->isforever, steppers[i]->speed(), steppers[i]->maxSpeed(), data[i]->targetPosition, data[i]->absolutePosition, data[i]->acceleration);
			//  run in background
			if (checkIfMinMaxPosIsReached(arraypos, i))
				return;
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
					sendMotorPos(i, arraypos);
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
					// Serial.println("Speed (accel) " + String(steppers[i]->speed()));
					Serial.println("Distance to go (accel)" + String(i) + " - "+ String(steppers[i]->distanceToGo()));
					steppers[i]->run();
				}
				else // run accelerated
				{
					Serial.println("Distance to go " + String(i) + " - "+ String(steppers[i]->distanceToGo()));
					// Serial.println("Speed (non accel) " + String(steppers[i]->speed()));
					steppers[i]->runSpeedToPosition();
				}
			}
		}
	}
}

void FocusMotor::updateWebSocket(int arraypos)
{
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

	if (false and isShareEnable)
	{
		disableEnablePin(-1);
	}
}

void FocusMotor::sendMotorPos(int i, int arraypos)
{
	DynamicJsonDocument doc(4096);
	doc[key_steppers][arraypos][key_stepperid] = i;
	doc[key_steppers][arraypos][key_position] = pins[i]->current_position;
	doc[key_steppers][arraypos]["isDone"] = true;
	arraypos++;
	// SerialProcess::serialize(doc);
	Serial.println("++");
	serializeJson(doc, Serial);
	Serial.println();
	Serial.println("--");
	// WifiController::sendJsonWebSocketMsg(doc); // FIXME: Only if socket available?
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
	log_i("stop stepper:%i", i);
	steppers[i]->stop();
	data[i]->isforever = false;
	data[i]->speed = 0;
	pins[i]->current_position = steppers[i]->currentPosition();
	data[i]->stopped = true;
	// disableEnablePin(i);
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
	return false;
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
