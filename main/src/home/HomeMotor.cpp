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
		serialize(moduleController.get(AvailableModules::home)->act(deserialize()));
	}

	void HomeMotor_get()
	{
		serialize(moduleController.get(AvailableModules::home)->get(deserialize()));
	}

	void HomeMotor_set()
	{
		serialize(moduleController.get(AvailableModules::home)->set(deserialize()));
	}
}
/*
Handle REST calls to the HomeMotor module
*/
int HomeMotor::act(DynamicJsonDocument j)
{
	if (DEBUG)
		Serial.println("home_act_fct");

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

	DynamicJsonDocument doc(4096); //StaticJsonDocument<1024> doc; // create return doc

	// add the home data to the json
	doc[key_home][key_steppers][0][key_home_timeout] = hdata[Stepper::A]->homeTimeout;
	doc[key_home][key_steppers][0][key_home_speed] = hdata[Stepper::A]->homeSpeed;
	doc[key_home][key_steppers][0][key_home_maxspeed] = hdata[Stepper::A]->homeMaxspeed;
	doc[key_home][key_steppers][0][key_home_direction] = hdata[Stepper::A]->homeDirection;
	doc[key_home][key_steppers][0][key_home_timestarted] = hdata[Stepper::A]->homeTimeStarted;
	doc[key_home][key_steppers][0][key_home_isactive] = hdata[Stepper::A]->homeIsActive;

	doc[key_home][key_steppers][1][key_home_timeout] = hdata[Stepper::X]->homeTimeout;
	doc[key_home][key_steppers][1][key_home_speed] = hdata[Stepper::X]->homeSpeed;
	doc[key_home][key_steppers][1][key_home_maxspeed] = hdata[Stepper::X]->homeMaxspeed;
	doc[key_home][key_steppers][1][key_home_direction] = hdata[Stepper::X]->homeDirection;
	doc[key_home][key_steppers][1][key_home_timestarted] = hdata[Stepper::X]->homeTimeStarted;
	doc[key_home][key_steppers][1][key_home_isactive] = hdata[Stepper::X]->homeIsActive;

	doc[key_home][key_steppers][2][key_home_timeout] = hdata[Stepper::Y]->homeTimeout;
	doc[key_home][key_steppers][2][key_home_speed] = hdata[Stepper::Y]->homeSpeed;
	doc[key_home][key_steppers][2][key_home_maxspeed] = hdata[Stepper::Y]->homeMaxspeed;
	doc[key_home][key_steppers][2][key_home_direction] = hdata[Stepper::Y]->homeDirection;
	doc[key_home][key_steppers][2][key_home_timestarted] = hdata[Stepper::Y]->homeTimeStarted;
	doc[key_home][key_steppers][2][key_home_isactive] = hdata[Stepper::Y]->homeIsActive;

	doc[key_home][key_steppers][3][key_home_timeout] = hdata[Stepper::Z]->homeTimeout;
	doc[key_home][key_steppers][3][key_home_speed] = hdata[Stepper::Z]->homeSpeed;
	doc[key_home][key_steppers][3][key_home_maxspeed] = hdata[Stepper::Z]->homeMaxspeed;
	doc[key_home][key_steppers][3][key_home_direction] = hdata[Stepper::Z]->homeDirection;
	doc[key_home][key_steppers][3][key_home_timestarted] = hdata[Stepper::Z]->homeTimeStarted;
	doc[key_home][key_steppers][3][key_home_isactive] = hdata[Stepper::Z]->homeIsActive;

	log_i("home_get_fct done");
	serializeJsonPretty(doc, Serial);
	return doc;
}

int HomeMotor::set(DynamicJsonDocument ob)
{
	return 1;
}

void sendHomeDone(int axis){
	// send home done to client

	DynamicJsonDocument doc(4096); //StaticJsonDocument<256> doc;
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
		if (hdata[Stepper::X]->homeIsActive && (digitalin->digitalin_val_1 || hdata[Stepper::X]->homeTimeStarted + hdata[Stepper::X]->homeTimeout < millis()))
		{
			// stopping motor and going reversing direction to release endstops
			int speed = motor->data[Stepper::X]->speed;
			motor->steppers[Stepper::X]->stop();
			// blocks until stepper reached new position wich would be optimal outside of the endstep
			if (speed > 0)
				motor->steppers[Stepper::X]->move(-hdata[Stepper::X]->homeEndposRelease);
			else
				motor->steppers[Stepper::X]->move(hdata[Stepper::X]->homeEndposRelease);

			hdata[Stepper::X]->homeIsActive = false;
			motor->steppers[Stepper::X]->runToPosition();
			motor->steppers[Stepper::X]->setCurrentPosition(0);
			motor->steppers[Stepper::X]->setSpeed(0);
			motor->data[Stepper::X]->isforever = false;
			motor->steppers[Stepper::X]->stop();
			log_i("Home Motor X done");
			log_i("Distance to go X: %i", motor->steppers[Stepper::X]->distanceToGo());
			// send home done to client
			sendHomeDone(1);
		}
		if (hdata[Stepper::Y]->homeIsActive && (digitalin->digitalin_val_2 || hdata[Stepper::Y]->homeTimeStarted + hdata[Stepper::Y]->homeTimeout < millis()))
		{
			int speed = motor->data[Stepper::Y]->speed;
			motor->steppers[Stepper::Y]->stop();
			if (speed > 0)
				motor->steppers[Stepper::Y]->move(-hdata[Stepper::Y]->homeEndposRelease);
			else
				motor->steppers[Stepper::Y]->move(hdata[Stepper::Y]->homeEndposRelease);
			motor->steppers[Stepper::Y]->runToPosition();
			hdata[Stepper::Y]->homeIsActive = false;
			motor->steppers[Stepper::Y]->setCurrentPosition(0);
			motor->steppers[Stepper::Y]->setSpeed(0);
			motor->data[Stepper::Y]->isforever = false;
			log_i("Home Motor Y done");
			log_i("Distance to go Y: %i", motor->steppers[Stepper::Y]->distanceToGo());
			// send home done to client
			sendHomeDone(2);
			log_i("Distance to go Y: %i", motor->steppers[Stepper::X]->distanceToGo());
		}
		if (hdata[Stepper::Z]->homeIsActive && (digitalin->digitalin_val_3 || hdata[Stepper::Z]->homeTimeStarted + hdata[Stepper::Z]->homeTimeout < millis()))
		{
			int speed = motor->data[Stepper::Z]->speed;
			motor->steppers[Stepper::Z]->stop();
			if (speed > 0)
				motor->steppers[Stepper::Z]->move(-hdata[Stepper::Z]->homeEndposRelease);
			else
				motor->steppers[Stepper::Z]->move(hdata[Stepper::Z]->homeEndposRelease);
			motor->steppers[Stepper::Z]->runToPosition();
			hdata[Stepper::Z]->homeIsActive = false;
			motor->steppers[Stepper::Z]->setCurrentPosition(0);
			motor->steppers[Stepper::Z]->setSpeed(0);
			motor->data[Stepper::Z]->isforever = false;
			log_i("Home Motor Z done");
			log_i("Distance to go Z: %i", motor->steppers[Stepper::Z]->distanceToGo());
			// send home done to client
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
}
