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
		moduleController.get(AvailableModules::home)->act(deserialize());
		serialize();
	}

	void HomeMotor_get()
	{
		moduleController.get(AvailableModules::home)->get(deserialize());
		serialize();
	}

	void HomeMotor_set()
	{
		moduleController.get(AvailableModules::home)->set(deserialize());
		serialize();
	}
}
/*
Handle REST calls to the HomeMotor module
*/
//{"task":"/home_act", "home":{"steppers":[{"endpospin":1, "timeout":1000, "speed":1000, "direction":1]}}
void HomeMotor::act(JsonObject  j)
{
	if (DEBUG)
		Serial.println("home_act_fct");

	
	if ((j).containsKey(key_home)){
	if ((j)[key_home].containsKey(key_steppers))
	{
		Serial.println("contains key");
		for (int i = 0; i < (j)[key_home][key_steppers].size(); i++)
		{
			Stepper s = static_cast<Stepper>((j)[key_home][key_steppers][i][key_stepperid]);

			if ((j)[key_home][key_steppers][i].containsKey(key_home_timeout))
				hdata[s]->homeTimeout = (j)[key_home][key_steppers][i][key_home_timeout];			
			if (j[key_home][key_steppers][i].containsKey(key_home_speed))
				hdata[s]->homeSpeed = (j)[key_home][key_steppers][i][key_home_speed];
			if (j[key_home][key_steppers][i].containsKey(key_home_maxspeed))
				hdata[s]->homeMaxspeed = (j)[key_home][key_steppers][i][key_home_maxspeed];
			if (j[key_home][key_steppers][i].containsKey(key_home_direction))
				hdata[s]->homeDirection = j[key_home][key_steppers][i][key_home_direction];
			if (j[key_home][key_steppers][i].containsKey(key_home_endposrelease))
				hdata[s]->homeEndposRelease = j[key_home][key_steppers][i][key_home_endposrelease];

			// grab current time
			hdata[s]->homeTimeStarted = millis();
			hdata[s]->homeIsActive = true;

			//trigger go home by starting the motor in the right direction
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			motor->data[s]->isforever = true;
			motor->data[s]->speed = hdata[s]->homeSpeed;
			motor->data[s]->maxspeed = hdata[s]->homeMaxspeed;

			// now we will go into loop and need to stop once the button is hit or timeout is reached
			log_i("Home Data Motor %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i, homeEndosRelease %i", i, hdata[s]->homeTimeout, hdata[s]->homeDirection*hdata[s]->homeSpeed, hdata[s]->homeDirection*hdata[s]->homeSpeed, hdata[s]->homeDirection, hdata[s]->homeTimeStarted, hdata[s]->homeEndposRelease);
	
		}
	}
	}

	WifiController::getJDoc()->clear();
}

void HomeMotor::get(JsonObject ob)
{
	if (DEBUG)
		Serial.println("home_get_fct");
	DynamicJsonDocument *j = WifiController::getJDoc();

	// add the home data to the json
	(*j)[key_home][key_steppers][0][key_home_timeout] = hdata[Stepper::A]->homeTimeout;
	(*j)[key_home][key_steppers][0][key_home_speed] = hdata[Stepper::A]->homeSpeed;
	(*j)[key_home][key_steppers][0][key_home_maxspeed] = hdata[Stepper::A]->homeMaxspeed;
	(*j)[key_home][key_steppers][0][key_home_direction] = hdata[Stepper::A]->homeDirection;
	(*j)[key_home][key_steppers][0][key_home_timestarted] = hdata[Stepper::A]->homeTimeStarted;
	(*j)[key_home][key_steppers][0][key_home_isactive] = hdata[Stepper::A]->homeIsActive;

	(*j)[key_home][key_steppers][1][key_home_timeout] = hdata[Stepper::X]->homeTimeout;
	(*j)[key_home][key_steppers][1][key_home_speed] = hdata[Stepper::X]->homeSpeed;
	(*j)[key_home][key_steppers][1][key_home_maxspeed] = hdata[Stepper::X]->homeMaxspeed;
	(*j)[key_home][key_steppers][1][key_home_direction] = hdata[Stepper::X]->homeDirection;
	(*j)[key_home][key_steppers][1][key_home_timestarted] = hdata[Stepper::X]->homeTimeStarted;
	(*j)[key_home][key_steppers][1][key_home_isactive] = hdata[Stepper::X]->homeIsActive;

	(*j)[key_home][key_steppers][2][key_home_timeout] = hdata[Stepper::Y]->homeTimeout;
	(*j)[key_home][key_steppers][2][key_home_speed] = hdata[Stepper::Y]->homeSpeed;
	(*j)[key_home][key_steppers][2][key_home_maxspeed] = hdata[Stepper::Y]->homeMaxspeed;
	(*j)[key_home][key_steppers][2][key_home_direction] = hdata[Stepper::Y]->homeDirection;
	(*j)[key_home][key_steppers][2][key_home_timestarted] = hdata[Stepper::Y]->homeTimeStarted;
	(*j)[key_home][key_steppers][2][key_home_isactive] = hdata[Stepper::Y]->homeIsActive;

	(*j)[key_home][key_steppers][3][key_home_timeout] = hdata[Stepper::Z]->homeTimeout;
	(*j)[key_home][key_steppers][3][key_home_speed] = hdata[Stepper::Z]->homeSpeed;
	(*j)[key_home][key_steppers][3][key_home_maxspeed] = hdata[Stepper::Z]->homeMaxspeed;
	(*j)[key_home][key_steppers][3][key_home_direction] = hdata[Stepper::Z]->homeDirection;
	(*j)[key_home][key_steppers][3][key_home_timestarted] = hdata[Stepper::Z]->homeTimeStarted;
	(*j)[key_home][key_steppers][3][key_home_isactive] = hdata[Stepper::Z]->homeIsActive;

}

void HomeMotor::set(JsonObject ob)
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

		//expecting digitalin1 handling endstep for stepper X, digital2 stepper Y, digital3 stepper Z
		// 0=A , 1=X, 2=Y , 3=Z
		if(hdata[Stepper::X]->homeIsActive && ( digitalin->digitalin_val_1 || hdata[Stepper::X]->homeTimeStarted + hdata[Stepper::X]->homeTimeout < millis()))
		{
			// stopping motor and going reversing direction to release endstops
			int speed = motor->data[Stepper::X]->speed;
			motor->steppers[Stepper::X]->stop();
			//blocks until stepper reached new position wich would be optimal outside of the endstep
			if(speed > 0)
				motor->steppers[Stepper::X]->move(-hdata[Stepper::X]->homeEndposRelease);
			else
				motor->steppers[Stepper::X]->move(hdata[Stepper::X]->homeEndposRelease);
			motor->steppers[Stepper::X]->runToPosition();
			hdata[Stepper::X]->homeIsActive = false;
			motor->steppers[Stepper::X]->setCurrentPosition(0);
			motor->data[Stepper::X]->isforever = false;
			log_i("Home Motor X done");
		}
		if(hdata[Stepper::Y]->homeIsActive && (digitalin->digitalin_val_2 || hdata[Stepper::Y]->homeTimeStarted + hdata[Stepper::Y]->homeTimeout < millis()))
		{
			int speed = motor->data[Stepper::Y]->speed;
			motor->steppers[Stepper::X]->stop();
			if(speed > 0)
				motor->steppers[Stepper::Y]->move(-hdata[Stepper::Y]->homeEndposRelease);
			else
				motor->steppers[Stepper::Y]->move(hdata[Stepper::Y]->homeEndposRelease);
			motor->steppers[Stepper::Y]->runToPosition();
			hdata[Stepper::Y]->homeIsActive = false;
			motor->steppers[Stepper::Y]->setCurrentPosition(0);
			motor->data[Stepper::Y]->isforever = false;
			log_i("Home Motor Y done");
		}
		if(hdata[Stepper::Z]->homeIsActive && (digitalin->digitalin_val_3 || hdata[Stepper::Z]->homeTimeStarted + hdata[Stepper::Z]->homeTimeout < millis()))
		{
			int speed = motor->data[Stepper::Z]->speed;
			motor->steppers[Stepper::Z]->stop();
			if(speed > 0)
				motor->steppers[Stepper::Z]->move(-hdata[Stepper::Z]->homeEndposRelease);
			else
				motor->steppers[Stepper::Z]->move(hdata[Stepper::Z]->homeEndposRelease);
			motor->steppers[Stepper::Z]->runToPosition();
			hdata[Stepper::Z]->homeIsActive = false;
			motor->steppers[Stepper::Z]->setCurrentPosition(0);
			motor->data[Stepper::Z]->isforever = false;
			log_i("Home Motor Z done");
		}
	}
}

/*
not needed all stuff get setup inside motor and digitalin, but must get implemented
*/
void HomeMotor::setup()
{
	
	for (int i = 0; i < 4; i++)
  	{
    hdata[i] = new HomeData ();
	}
}