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
		// Immediately stop the motor
		for (int i = 0; i < steppers.size(); i++)
		{
			steppers[i]->stop();
			isforever = 0;
			setEnableMotor(false);
			data[i]->currentPosition = steppers[i]->currentPosition();
		}
		isforever = 0;
		setEnableMotor(false);

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
	setEnableMotor(true);
	for (int i = 0; i < steppers.size(); i++)
	{
		log_i("is data %i null:%s set speed/max %i/%i",i,boolToChar(data[i]==nullptr), data[i]->speed, data[i]->maxspeed);
		log_i("set speed/max %i/%i", data[i]->speed, data[i]->maxspeed);
		steppers[i]->setSpeed(data[i]->speed);
		steppers[i]->setMaxSpeed(data[i]->maxspeed);
		if (!isforever)
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

	if (DEBUG)
		Serial.println("Start rotation in background");

	(*WifiController::getJDoc())["POSA"] = data[Stepper::A]->currentPosition;
	(*WifiController::getJDoc())["POSX"] = data[Stepper::X]->currentPosition;
	(*WifiController::getJDoc())["POSY"] = data[Stepper::Y]->currentPosition;
	(*WifiController::getJDoc())["POSZ"] = data[Stepper::Z]->currentPosition;
}

void FocusMotor::setEnableMotor(bool enable)
{
	isBusy = enable;
	//digitalWrite(pins->ENABLE, !enable);
	motor_enable = enable;
}

bool FocusMotor::getEnableMotor()
{
	return motor_enable;
}

void FocusMotor::set()
{
	// default value handling
	int axis = -1;
	if (WifiController::getJDoc()->containsKey("axis"))
	{
		axis = (*WifiController::getJDoc())["axis"];
	}

	int currentposition = NULL;
	if (WifiController::getJDoc()->containsKey("currentposition"))
	{
		currentposition = (*WifiController::getJDoc())["currentposition"];
	}

	int maxspeed = NULL;
	if (WifiController::getJDoc()->containsKey("maxspeed"))
	{
		maxspeed = (*WifiController::getJDoc())["maxspeed"];
	}

	int accel = NULL;
	if (WifiController::getJDoc()->containsKey("accel"))
	{
		accel = (*WifiController::getJDoc())["accel"];
	}

	int pinstep = -1;
	if (WifiController::getJDoc()->containsKey("pinstep"))
	{
		pinstep = (*WifiController::getJDoc())["pinstep"];
	}

	int pindir = -1;
	if (WifiController::getJDoc()->containsKey("pindir"))
	{
		pindir = (*WifiController::getJDoc())["pindir"];
	}

	int isen = -1;
	if (WifiController::getJDoc()->containsKey("isen"))
	{
		isen = (*WifiController::getJDoc())["isen"];
	}

	int sign = NULL;
	if (WifiController::getJDoc()->containsKey("sign"))
	{
		sign = (*WifiController::getJDoc())["sign"];
	}

	// DEBUG printing
	if (DEBUG)
	{
		Serial.print("axis ");
		Serial.println(axis);
		Serial.print("currentposition ");
		Serial.println(currentposition);
		Serial.print("maxspeed ");
		Serial.println(maxspeed);
		Serial.print("pinstep ");
		Serial.println(pinstep);
		Serial.print("pindir ");
		Serial.println(pindir);
		Serial.print("isen ");
		Serial.println(isen);
		Serial.print("accel ");
		Serial.println(accel);
		Serial.print("isen: ");
		Serial.println(isen);
	}

	if (accel >= 0)
	{
		if (DEBUG)
			Serial.print("accel ");
		Serial.println(accel);
		steppers[axis]->setAcceleration(accel);
	}

	if (sign != NULL)
	{
		if (DEBUG)
			Serial.print("sign ");
		Serial.println(sign);
		data[axis]->SIGN = sign;
	}

	if (currentposition != NULL)
	{
		if (DEBUG)
			Serial.print("currentposition ");
		Serial.println(currentposition);
		data[axis]->currentPosition = currentposition;
	}
	if (maxspeed != 0)
	{
		data[axis]->maxspeed = maxspeed;
		steppers[axis]->setMaxSpeed(maxspeed);
	}
	if (pindir != 0 and pinstep != 0)
	{
		if (axis == 0)
		{
			pins->STEP_A = pinstep;
			pins->DIR_A = pindir;
		}
		else if (axis == 1)
		{
			pins->STEP_X = pinstep;
			pins->DIR_X = pindir;
		}
		else if (axis == 2)
		{
			pins->STEP_Y = pinstep;
			pins->DIR_Y = pindir;
		}
		else if (axis == 3)
		{
			pins->STEP_Z = pinstep;
			pins->DIR_Z = pindir;
		}
	}

	// if (DEBUG) Serial.print("isen "); Serial.println(isen);
	/*if (isen != 0 and isen)
	{
		digitalWrite(pins->ENABLE, 0);
	}
	else if (isen != 0 and not isen)
	{
		digitalWrite(pins->ENABLE, 1);
	}*/
	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["return"] = 1;
}

void FocusMotor::get()
{
	int axis = (*WifiController::getJDoc())["axis"];
	if (DEBUG)
		Serial.println("motor_get_fct");
	if (DEBUG)
		Serial.println(axis);

	int mmaxspeed = 0;
	int pinstep = 0;
	int pindir = 0;
	int sign = 0;
	int mspeed = 0;

	mmaxspeed = steppers[axis]->maxSpeed();
	mspeed = steppers[axis]->speed();
	data[axis]->currentPosition = steppers[axis]->currentPosition();
	pinstep = pins->STEP_X;
	pindir = pins->DIR_X;
	sign = SIGN_X;

	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["position"] = data[axis]->currentPosition;
	(*WifiController::getJDoc())["speed"] = steppers[axis]->speed();
	(*WifiController::getJDoc())["maxspeed"] = steppers[axis]->maxSpeed();
	(*WifiController::getJDoc())["pinstep"] = pinstep;
	(*WifiController::getJDoc())["pindir"] = pindir;
	(*WifiController::getJDoc())["sign"] = sign;
}

void FocusMotor::setup(PINDEF *pins)
{
	this->pins = pins;
	steppers[Stepper::A] = new AccelStepper(AccelStepper::DRIVER, pins->STEP_A, pins->DIR_A);
	steppers[Stepper::X] = new AccelStepper(AccelStepper::DRIVER, pins->STEP_X, pins->DIR_X);
	steppers[Stepper::Y] = new AccelStepper(AccelStepper::DRIVER, pins->STEP_Y, pins->DIR_Y);
	steppers[Stepper::Z] = new AccelStepper(AccelStepper::DRIVER, pins->STEP_Z, pins->DIR_Z);
	data[Stepper::A] = new MotorData();
	data[Stepper::X] = new MotorData();
	data[Stepper::Y] = new MotorData();
	data[Stepper::Z] = new MotorData();
	/*
	   Motor related settings
	*/
	Serial.println("Setting Up Motors");
	//pinMode(pins->ENABLE, OUTPUT);
	setEnableMotor(true);
	Serial.println("Setting Up Motor A,X,Y,Z");
	for (int i = 0; i < steppers.size(); i++)
	{
		steppers[i]->setMaxSpeed(MAX_VELOCITY_A);
		steppers[i]->setAcceleration(MAX_ACCELERATION_A);
		steppers[i]->enableOutputs();
		steppers[i]->runToNewPosition(-100);
		steppers[i]->runToNewPosition(100);
		steppers[i]->setCurrentPosition(0);
	}
	setEnableMotor(false);
}

bool FocusMotor::background()
{

	for (int i = 0; i < steppers.size(); i++)
	{
		//log_i("data %i isnull:%s stepper is null:%s", i,boolToChar(data[i] == nullptr), boolToChar(steppers[i] == nullptr));
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
	}

	// checks if a stepper is still running
	bool motordrive = false;
	for (int i = 0; i < steppers.size(); i++)
	{
		if (steppers[i]->distanceToGo() > 0)
		{
			motordrive = true;
		}
		else
			steppers[i]->stop();
	}

	// all steppers reached their positions? yeah turn motor off..
	if (!isen && !motordrive)
	{
		setEnableMotor(false);
	}
	isBusy = false;
	return true;
}

FocusMotor motor;

#endif
