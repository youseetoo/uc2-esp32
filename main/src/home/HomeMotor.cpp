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
//{"task":"/home_act", "home":{"steppers":[{"endpospin":1, "timeout":1000, "speed":1000, "direction":1]}}
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
			Stepper s = static_cast<Stepper>((*j)[key_home][key_steppers][i][key_stepperid]);

			if ((*j)[key_home][key_steppers][i].containsKey(key_home_timeout))
				hdata[s]->homeTimeout = (*j)[key_home][key_steppers][i][key_home_timeout];			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_speed))
				hdata[s]->homeSpeed = (*j)[key_home][key_steppers][i][key_home_speed];
			Serial.println("speed");
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_maxspeed))
				hdata[s]->homeMaxspeed = (*j)[key_home][key_steppers][i][key_home_maxspeed];
			Serial.println("maxspeed");
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_direction))
				hdata[s]->homeDirection = (*j)[key_home][key_steppers][i][key_home_direction];
			
			// grab current time
			hdata[s]->homeTimeStarted = millis();
			Serial.println("direction");
			hdata[s]->homeIsActive = true;

			//trigger go home by starting the motor in the right direction
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			Serial.println("motor");
			motor->steppers[i]->setSpeed(hdata[s]->homeDirection*hdata[s]->homeSpeed);
			motor->steppers[i]->setMaxSpeed(hdata[s]->homeMaxspeed);
			motor->steppers[i]->runSpeed();

			// now we will go into loop and need to stop once the button is hit or timeout is reached
			
			serializeJsonPretty((*j), Serial);
			Serial.println(s);
			log_i("Home Data Motor %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i", i, hdata[s]->homeTimeout, hdata[s]->homeSpeed, hdata[s]->homeMaxspeed, hdata[s]->homeDirection, hdata[s]->homeTimeStarted);
	
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




/*
	get called repeatedly, dont block this
*/
void HomeMotor::loop()
{
	// this will be called everytime, so we need to make this optional with a switch
	if (moduleController.get(AvailableModules::motor) != nullptr && moduleController.get(AvailableModules::digitalin) != nullptr)
	{	
		// get motor and switch instances
		FocusMotor * motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		
		DigitalInController * digitalin = (DigitalInController*)moduleController.get(AvailableModules::digitalin);

		Serial.print("digitalin loop: ");
		Serial.print(digitalin->digitalin_val_1);
		Serial.print(digitalin->digitalin_val_2);
		Serial.println(digitalin->digitalin_val_3);
		//expecting digitalin1 handling endstep for stepper X, digital2 stepper Y, digital3 stepper Z
		if(hdata[Stepper::X]->homeIsActive && ( digitalin->digitalin_val_1 || hdata[Stepper::X]->homeTimeStarted + hdata[Stepper::X]->homeTimeout < millis()))
		{
			// stopping motor and going reversing direction to release endstops
			int speed = motor->data[Stepper::X]->speed;
			motor->steppers[Stepper::X]->stop();
			//blocks until stepper reached new position wich would be optimal outside of the endstep
			if(speed > 0)
				motor->steppers[Stepper::X]->runToNewPosition(homeEndposRelease);
			else
				motor->steppers[Stepper::X]->runToNewPosition(homeEndposRelease);
			hdata[Stepper::X]->homeIsActive = false;
			motor->steppers[Stepper::X]->setCurrentPosition(0);
			log_i("Home Motor X done");
		}
		if(hdata[Stepper::Y]->homeIsActive && (digitalin->digitalin_val_2 || hdata[Stepper::Y]->homeTimeStarted + hdata[Stepper::Y]->homeTimeout < millis()))
		{
			int speed = motor->data[Stepper::Y]->speed;
			motor->steppers[Stepper::X]->stop();
			if(speed > 0)
				motor->steppers[Stepper::Y]->runToNewPosition(-homeEndposRelease);
			else
				motor->steppers[Stepper::Y]->runToNewPosition(homeEndposRelease);
			hdata[Stepper::Y]->homeIsActive = false;
			motor->steppers[Stepper::Y]->setCurrentPosition(0);
			log_i("Home Motor Y done");
		}
		if(hdata[Stepper::Z]->homeIsActive && (digitalin->digitalin_val_3 || hdata[Stepper::Z]->homeTimeStarted + hdata[Stepper::Z]->homeTimeout < millis()))
		{
			int speed = motor->data[Stepper::Z]->speed;
			motor->steppers[Stepper::Z]->stop();
			if(speed > 0)
				motor->steppers[Stepper::Z]->runToNewPosition(-homeEndposRelease);
			else
				motor->steppers[Stepper::Z]->runToNewPosition(homeEndposRelease);
			hdata[Stepper::Z]->homeIsActive = false;
			motor->steppers[Stepper::Z]->setCurrentPosition(0);
			log_i("Home Motor Z done");
		}
	}
}

/*
not needed all stuff get setup inside motor and digitalin, but must get implemented
*/
void HomeMotor::setup()
{}