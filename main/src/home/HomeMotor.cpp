#include "HomeMotor.h"
#include "../motor/FocusMotor.h"
#include "FastAccelStepper.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"

HomeMotor::HomeMotor() : Module() { log_i("ctor"); }
HomeMotor::~HomeMotor() { log_i("~ctor"); }


void processHomeLoop(void *p)
{
	Module *m = moduleController.get(AvailableModules::home);
}

/*
Handle REST calls to the HomeMotor module
*/
int HomeMotor::act(DynamicJsonDocument j)
{
	if (DEBUG)
		Serial.println("home_act_fct");

	// set position
	if ((j).containsKey(key_setposition))
	{
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		if ((j)[key_setposition].containsKey(key_steppers))
		{
			for (int i = 0; i < (j)[key_setposition][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>((j)[key_setposition][key_steppers][i][key_stepperid]);
				long pos = (j)[key_setposition][key_steppers][i][key_currentpos];
				motor->faststeppers[s]->setCurrentPosition(pos);
				log_d("set position %d to %d", s, pos);
			}
		}
	}

	// initiate homing
	if ((j).containsKey(key_home))
	{
		if ((j)[key_home].containsKey(key_steppers))
		{
			for (int i = 0; i < (j)[key_home][key_steppers].size(); i++)
			{
				Stepper s = static_cast<Stepper>((j)[key_home][key_steppers][i][key_stepperid]);

				// set timeout until the motor should seek for the endstop
				if ((j)[key_home][key_steppers][i].containsKey(key_home_timeout))
					hdata[s]->homeTimeout = (j)[key_home][key_steppers][i][key_home_timeout];
				// set speed of the motor while searching for the endstop
				if (j[key_home][key_steppers][i].containsKey(key_home_speed))
					hdata[s]->homeSpeed = (j)[key_home][key_steppers][i][key_home_speed];
				// set max speed of the motor while searching for the endstop
				if (j[key_home][key_steppers][i].containsKey(key_home_maxspeed))
					hdata[s]->homeMaxspeed = (j)[key_home][key_steppers][i][key_home_maxspeed];
				// set the direction in which the motor should walk to the endstop
				if (j[key_home][key_steppers][i].containsKey(key_home_direction))
					hdata[s]->homeDirection = j[key_home][key_steppers][i][key_home_direction];
				// how much should we move in the opposite direction compared to the endstop?
				if (j[key_home][key_steppers][i].containsKey(key_home_endposrelease))
					hdata[s]->homeEndposRelease = j[key_home][key_steppers][i][key_home_endposrelease];
				// polarity of endstop signal
				if (j[key_home][key_steppers][i].containsKey(key_home_endstoppolarity))
					hdata[s]->homeEndStopPolarity = j[key_home][key_steppers][i][key_home_endstoppolarity];

				// grab current time
				hdata[s]->homeTimeStarted = millis();
				hdata[s]->homeIsActive = true;

				// trigger go home by starting the motor in the right direction
				FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
				motor->data[s]->isforever = true;
				motor->data[s]->speed = hdata[s]->homeDirection * hdata[s]->homeSpeed;
				motor->data[s]->maxspeed = hdata[s]->homeDirection * hdata[s]->homeMaxspeed;

				// now we will go into loop and need to stop once the button is hit or timeout is reached
				log_i("Home Data Motor %i, Axis: %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i, homeEndosRelease %i",
					  i, s, hdata[s]->homeTimeout, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection, hdata[s]->homeTimeStarted, hdata[s]->homeEndposRelease);

				motor->startStepper(s);
			}
		}
	}

	j.clear();
	return 1;
}

DynamicJsonDocument HomeMotor::get(DynamicJsonDocument ob)
{
	log_i("home_get_fct");
	ob.clear();

	DynamicJsonDocument doc(4096); // StaticJsonDocument<1024> doc; // create return doc

	// add the home data to the json

	for(int i =0; i < 4;i++)
	{
		doc[key_home][key_steppers][i][key_home_timeout] = hdata[i]->homeTimeout;
		doc[key_home][key_steppers][i][key_home_speed] = hdata[i]->homeSpeed;
		doc[key_home][key_steppers][i][key_home_maxspeed] = hdata[i]->homeMaxspeed;
		doc[key_home][key_steppers][i][key_home_direction] = hdata[i]->homeDirection;
		doc[key_home][key_steppers][i][key_home_timestarted] = hdata[i]->homeTimeStarted;
		doc[key_home][key_steppers][i][key_home_isactive] = hdata[i]->homeIsActive;
	}

	log_i("home_get_fct done");
	serializeJsonPretty(doc, Serial);
	return doc;
}

void sendHomeDone(int axis)
{
	// send home done to client

	DynamicJsonDocument doc(4096); // StaticJsonDocument<256> doc;
	JsonObject home_steppers = doc["home"]["steppers"].createNestedObject();
	home_steppers["axis"] = axis;
	home_steppers["isDone"] = true;
	Serial.println("++");
	serializeJson(doc, Serial);
	Serial.println();
	Serial.println("--");
}

/*
	get called repeatedly, dont block this
*/
void HomeMotor::loop()
{

	// this will be called everytime, so we need to make this optional with a switch
	if (moduleController.get(AvailableModules::motor) != nullptr && moduleController.get(AvailableModules::digitalin) != nullptr)
	{
		// get motor and switch instances
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		DigitalInController *digitalin = (DigitalInController *)moduleController.get(AvailableModules::digitalin);

		// expecting digitalin1 handling endstep for stepper X, digital2 stepper Y, digital3 stepper Z
		//  0=A , 1=X, 2=Y , 3=Z
		if (hdata[Stepper::X]->homeIsActive && (abs(hdata[Stepper::X]->homeEndStopPolarity-digitalin->digitalin_val_1) || hdata[Stepper::X]->homeTimeStarted + hdata[Stepper::X]->homeTimeout < millis()))
		{
			// stopping motor and going reversing direction to release endstops
			int speed = motor->data[Stepper::X]->speed;
			motor->faststeppers[Stepper::X]->forceStop();
			// blocks until stepper reached new position wich would be optimal outside of the endstep
			if (speed > 0)
				motor->faststeppers[Stepper::X]->move(-hdata[Stepper::X]->homeEndposRelease);
			else
				motor->faststeppers[Stepper::X]->move(hdata[Stepper::X]->homeEndposRelease);
			// wait until stepper reached new position
			while (motor->faststeppers[Stepper::X]->getCurrentPosition() - motor->faststeppers[Stepper::X]->targetPos()) delay(1);
			hdata[Stepper::X]->homeIsActive = false;
			motor->faststeppers[Stepper::X]->setCurrentPosition(0);
			motor->faststeppers[Stepper::X]->setSpeedInHz(0);
			motor->data[Stepper::X]->isforever = false;
			motor->faststeppers[Stepper::X]->forceStop();
			log_i("Home Motor X done");
			sendHomeDone(1);
		}
		if (hdata[Stepper::Y]->homeIsActive && (abs(hdata[Stepper::Y]->homeEndStopPolarity-digitalin->digitalin_val_2)|| hdata[Stepper::Y]->homeTimeStarted + hdata[Stepper::Y]->homeTimeout < millis()))
		{
			int speed = motor->data[Stepper::Y]->speed;
			motor->faststeppers[Stepper::Y]->forceStop();
			if (speed > 0)
				motor->faststeppers[Stepper::Y]->move(-hdata[Stepper::Y]->homeEndposRelease);
			else
				motor->faststeppers[Stepper::Y]->move(hdata[Stepper::Y]->homeEndposRelease);
			// wait until stepper reached new position
			while (motor->faststeppers[Stepper::Y]->getCurrentPosition() - motor->faststeppers[Stepper::Y]->targetPos()) delay(1);
			hdata[Stepper::Y]->homeIsActive = false;
			motor->faststeppers[Stepper::Y]->setCurrentPosition(0);
			motor->faststeppers[Stepper::Y]->setSpeedInHz(0);
			motor->data[Stepper::Y]->isforever = false;
			log_i("Home Motor Y done");
			// log_i("Distance to go Y: %i", motor->faststeppers[Stepper::Y]->distanceToGo());
			//  send home done to client
			sendHomeDone(2);
			// log_i("Distance to go Y: %i", motor->faststeppers[Stepper::X]->distanceToGo());
		}
		if (hdata[Stepper::Z]->homeIsActive && (abs(hdata[Stepper::Z]->homeEndStopPolarity-digitalin->digitalin_val_3) || hdata[Stepper::Z]->homeTimeStarted + hdata[Stepper::Z]->homeTimeout < millis()))
		{
			int speed = motor->data[Stepper::Z]->speed;
			motor->faststeppers[Stepper::Z]->forceStop();
			if (speed > 0)
				motor->faststeppers[Stepper::Z]->move(-hdata[Stepper::Z]->homeEndposRelease);
			else
				motor->faststeppers[Stepper::Z]->move(hdata[Stepper::Z]->homeEndposRelease);
			// wait until stepper reached new position
			while (motor->faststeppers[Stepper::Z]->getCurrentPosition() - motor->faststeppers[Stepper::Z]->targetPos()) delay(1);				
			hdata[Stepper::Z]->homeIsActive = false;
			motor->faststeppers[Stepper::Z]->setCurrentPosition(0);
			motor->faststeppers[Stepper::Z]->setSpeedInHz(0);
			motor->data[Stepper::Z]->isforever = false;
			log_i("Home Motor Z done");
			// log_i("Distance to go Z: %i", motor->faststeppers[Stepper::Z]->distanceToGo());
			//  send home done to client
			sendHomeDone(3);
		}
	}
}

/*
not needed all stuff get setup inside motor and digitalin, but must get implemented
*/
void HomeMotor::setup()
{
	log_i("HomeMotor setup");
	for (int i = 0; i < 4; i++)
	{
		hdata[i] = new HomeData();
	}
	// xTaskCreate(&processHomeLoop, "home_task", 1024, NULL, 5, NULL);
}
