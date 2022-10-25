#include "../../config.h"
#include "HomeMotor.h"


HomeMotor::HomeMotor() : Module() { log_i("ctor"); }
HomeMotor::~HomeMotor() { log_i("~ctor"); }

/*
Trigger the HomeMotor to move to the home position (?)
*/
namespace RestApi
{
	void HomeMotor_act()
	{
		deserialize();
		moduleController.get(AvailableModules::home)->act();
		serialize();
	}

	void HomeMotor_get()
	{
		deserialize();
		moduleController.get(AvailableModules::home)->get();
		serialize();
	}

	void HomeMotor_set()
	{
		deserialize();
		moduleController.get(AvailableModules::home)->set();
		serialize();
	}
}
/*
Handle REST calls to the HomeMotor module
*/

void HomeMotor::act()
{
	if (DEBUG)
		Serial.println("home_act_fct");
	DynamicJsonDocument *j = WifiController::getJDoc();

	if ((*j).containsKey(key_home)){
	if ((*j)[key_home].containsKey(key_steppers))
	{
		Serial.println("contains key");
		for (int i = 0; i < (*j)[key_home][key_steppers].size(); i++)
		{
			Serial.println(i);
			Stepper s = static_cast<Stepper>((*j)[key_home][key_steppers][i][key_stepperid]);
			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_endpospin))
				hdata[s]->homeEndposPin = (*j)[key_home][key_steppers][i][key_home_endpospin];
			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_timeout))
				hdata[0]->homeTimeout = (*j)[key_home][key_steppers][i][key_home_timeout];
			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_speed))
				hdata[s]->homeSpeed = (*j)[key_home][key_steppers][i][key_home_speed];
			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_maxspeed))
				hdata[s]->homeMaxspeed = (*j)[key_home][key_steppers][i][key_home_maxspeed];
			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_direction))
				hdata[s]->homeDirection = (*j)[key_home][key_steppers][i][key_home_direction];
			
			// grab current time
			hdata[0]->homeTimeStarted = millis();
			
			//trigger go home by starting the motor in the right direction
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			motor->steppers[i]->setSpeed(hdata[i]->homeDirection*hdata[i]->homeSpeed);
			motor->steppers[i]->setMaxSpeed(hdata[i]->homeMaxspeed);
			motor->steppers[i]->runSpeed();

			// now we will go into loop and need to stop once the button is hit or timeout is reached
			
	
		}
	}
	}

	WifiController::getJDoc()->clear();
}

void HomeMotor::get()
{
	
}
void HomeMotor::set()
{
	
}

// {"task":"/home_act", "home": {"home":1, "steppers": [{"id":0, "endpospin": 0, "timeout": 10000, "speed": 1000, "direction":1}]}}


/*
	get called reapting, dont block this
*/
void HomeMotor::loop()
{
	//check if motor and digitalin is avail
	if (moduleController.get(AvailableModules::motor) != nullptr && moduleController.get(AvailableModules::digitalin) != nullptr)
	{
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		DigitalInController * digitalin = (DigitalInController*)moduleController.get(AvailableModules::digitalin);
		//if one of the endpoints returned > 0 all drives get stopped
		// or if the timeout is reached all drives get stopped
		if (digitalin->digitalin_val_1 || digitalin->digitalin_val_2 || digitalin->digitalin_val_3 || hdata[0]->homeTimeStarted + hdata[0]->homeTimeout < millis())
		{
			for (int i = 0; i < motor->steppers.size(); i++)
			{
				motor->steppers[i]->stop();
			}
		}
		{
			motor->stopAllDrives();
			// TODO: we need to move the motor by N-steps into the other direction so that the Switch is not blocked and now motor will move
			// Each homing event will trigger only one motor axis.
			//how can we get the current motor axis?
		}
	}
}

/*
not needed all stuff get setup inside motor and digitalin, but must get implemented
*/
void HomeMotor::setup()
{}