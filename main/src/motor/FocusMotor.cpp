#include "../../config.h"
#ifdef IS_MOTOR
#include "FocusMotor.h"
#include <ArduinoJson.h>

void FocusMotor::act()
{
	if (DEBUG)
		Serial.println("motor_act_fct");

	if (WifiController::getJDoc()->containsKey(key_motor))
	{
		if ((*WifiController::getJDoc())[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < (*WifiController::getJDoc())[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>((*WifiController::getJDoc())[key_motor][key_steppers][i][key_stepperid]);
				if ((*WifiController::getJDoc())[key_motor][key_steppers][i].containsKey(key_speed))
					data[s]->speed = (*WifiController::getJDoc())[key_motor][key_steppers][i][key_speed];
				if ((*WifiController::getJDoc())[key_motor][key_steppers][i].containsKey(key_position))
					data[s]->targetPosition = (*WifiController::getJDoc())[key_motor][key_steppers][i][key_position];
			}
		}
		isabs = (*WifiController::getJDoc())[key_motor].containsKey(key_isabs) ? (*WifiController::getJDoc())[key_motor][key_isabs] : 0;
		isstop = (*WifiController::getJDoc())[key_motor].containsKey(key_isstop) ? (*WifiController::getJDoc())[key_motor][key_isstop] : 0;
		isaccel = (*WifiController::getJDoc())[key_motor].containsKey(key_isaccel) ? (*WifiController::getJDoc())[key_motor][key_isaccel] : 0;
		isen = (*WifiController::getJDoc())[key_motor].containsKey(key_isen) ? (*WifiController::getJDoc())[key_motor][key_isen] : 0;
		isforever = (*WifiController::getJDoc())[key_motor].containsKey(key_isforever) ? (*WifiController::getJDoc())[key_motor][key_isforever] : 0;
	}

	WifiController::getJDoc()->clear();

	if (DEBUG)
	{
		for (int i = 0; i < data.size(); i++)
		{
			log_i("data %i: speed:%i target position:%i", i, data[i]->speed, data[i]->targetPosition);
			log_i("isabs:%s isen:%s isstop:%s isaccel:%s isforever:%s", boolToChar(isabs), boolToChar(isen), boolToChar(isstop), boolToChar(isaccel), boolToChar(isforever));
		}
	}

	if (isstop)
	{
		stopAllDrives();
		(*WifiController::getJDoc())["POSA"] = data[Stepper::A]->currentPosition;
		(*WifiController::getJDoc())["POSX"] = data[Stepper::X]->currentPosition;
		(*WifiController::getJDoc())["POSY"] = data[Stepper::Y]->currentPosition;
		(*WifiController::getJDoc())["POSZ"] = data[Stepper::Z]->currentPosition;
		return;
	}

#if defined IS_PS3 || defined IS_PS4
	if (ps_c.IS_PSCONTROLER_ACTIVE)
		ps_c.IS_PSCONTROLER_ACTIVE = false; // override PS controller settings #TODO: Somehow reset it later?
#endif
	// prepare motor to run
	startAllDrives();

	if (DEBUG)
		Serial.println("Start rotation in background");

	(*WifiController::getJDoc())["POSA"] = data[Stepper::A]->currentPosition;
	(*WifiController::getJDoc())["POSX"] = data[Stepper::X]->currentPosition;
	(*WifiController::getJDoc())["POSY"] = data[Stepper::Y]->currentPosition;
	(*WifiController::getJDoc())["POSZ"] = data[Stepper::Z]->currentPosition;
}

void FocusMotor::set()
{
	DynamicJsonDocument * doc = WifiController::getJDoc();
	if (doc->containsKey(key_motor))
	{
		if ((*doc)[key_motor].containsKey(key_steppers))
		{
			for (int i = 0; i < (*doc)[key_motor][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>((*doc)[key_motor][key_steppers][i][key_stepperid]);
				pins[i].DIR = (*doc)[key_motor][key_steppers][i][key_dir];
				pins[i].STEP = (*doc)[key_motor][key_steppers][i][key_step];
				pins[i].ENABLE = (*doc)[key_motor][key_steppers][i][key_enable];
				pins[i].direction_inverted = (*doc)[key_motor][key_steppers][i][key_dir_inverted];
				pins[i].step_inverted = (*doc)[key_motor][key_steppers][i][key_step_inverted];
				pins[i].enable_inverted = (*doc)[key_motor][key_steppers][i][key_enable_inverted];
			}
			Config::setMotorPinConfig(false);
			setup();
		}
	}
	doc->clear();
}

void FocusMotor::get()
{
	int axis = (*WifiController::getJDoc())["axis"];
	if (DEBUG)
		Serial.println("motor_get_fct");
	if (DEBUG)
		Serial.println(axis);
	data[axis]->currentPosition = steppers[axis]->currentPosition();

	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["position"] = data[axis]->currentPosition;
	(*WifiController::getJDoc())["speed"] = steppers[axis]->speed();
	(*WifiController::getJDoc())["maxspeed"] = steppers[axis]->maxSpeed();
	(*WifiController::getJDoc())["pinstep"] = pins[axis].STEP;
	(*WifiController::getJDoc())["pindir"] = pins[axis].DIR;
	(*WifiController::getJDoc())["sign"] = data[axis]->SIGN;
}

void FocusMotor::setup()
{
	// inti clean pin and motordata
	for (int i = 0; i < 4; i++)
	{
		data[i] = new MotorData();
	}
	// get pins from config
	Config::getMotorPins();
	// create the stepper
	
	for (int i = 0; i < 4; i++)
	{
		log_i("Pins: Step: %i Dir: %i Enable:%i", pins[i].STEP, pins[i].DIR, pins[i].ENABLE);
		steppers[i] = new AccelStepper(AccelStepper::DRIVER, pins[i].STEP, pins[i].DIR);
		steppers[i]->setEnablePin(pins[i].ENABLE);
		steppers[i]->setPinsInverted(pins[i].step_inverted,pins[i].direction_inverted,pins[i].enable_inverted);
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
		steppers[i]->setCurrentPosition(0);
		steppers[i]->disableOutputs();
	}
}

bool FocusMotor::background()
{
	for (int i = 0; i < steppers.size(); i++)
	{
		// log_i("data %i isnull:%s stepper is null:%s", i,boolToChar(data[i] == nullptr), boolToChar(steppers[i] == nullptr));
		data[i]->currentPosition = steppers[i]->currentPosition();
		if (isforever)
		{
			steppers[i]->setSpeed(data[i]->speed);
			steppers[i]->setMaxSpeed(data[i]->maxspeed);
			steppers[i]->runSpeed();
		}
		else
		{
			// run at constant speed
			if (isaccel)
			{
				steppers[i]->run();
			}
			else
			{
				steppers[i]->runSpeedToPosition();
			}
		}
		// checks if a stepper is still running
		if (steppers[i]->distanceToGo() == 0)
		{
			steppers[i]->disableOutputs();
		}
	}
	return true;
}

void FocusMotor::stopAllDrives()
{
	// Immediately stop the motor
	for (int i = 0; i < steppers.size(); i++)
	{
		steppers[i]->stop();
		data[i]->isforever = false;
		data[i]->currentPosition = steppers[i]->currentPosition();
		steppers[i]->disableOutputs();
	}
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
					pins[i].STEP, 
					pins[i].DIR, 
					pins[i].ENABLE, 
					boolToChar(steppers[i]->areOutputsEnabled()));
		if(!steppers[i]->areOutputsEnabled())
			steppers[i]->enableOutputs();
		steppers[i]->setSpeed(data[i]->speed);
		steppers[i]->setMaxSpeed(data[i]->maxspeed);
		if (!data[i]->isforever)
		{
			if (isabs)
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

FocusMotor motor;

#endif
