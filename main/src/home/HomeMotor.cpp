#include "HomeMotor.h"
#include "../motor/FocusMotor.h"
#include "FastAccelStepper.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "HardwareSerial.h"

HomeMotor::HomeMotor() : Module() { log_i("ctor"); }
HomeMotor::~HomeMotor() { log_i("~ctor"); }


void processHomeLoop(void *p)
{
	Module *m = moduleController.get(AvailableModules::home);
}

/*
Handle REST calls to the HomeMotor module
*/
int HomeMotor::act(cJSON * j)
{
	// set position
	cJSON * setpos = cJSON_GetObjectItem(j,key_setposition);
	if (DEBUG)
		Serial.println("home_act_fct");
	if(setpos != NULL)
	{
		cJSON * stprs = cJSON_GetObjectItem(setpos,key_steppers);
		if (stprs != NULL)
		{
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			cJSON * stp = NULL;
			cJSON_ArrayForEach(stp,stprs)
			{
				Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp,key_stepperid)->valueint);
				motor->faststeppers[s]->setCurrentPosition(cJSON_GetObjectItemCaseSensitive(stp,key_currentpos)->valueint);
			}
		}
		
	}
	
	cJSON * home = cJSON_GetObjectItem(j,key_home);
	if (home != NULL)
	{
		cJSON * stprs = cJSON_GetObjectItem(home,key_steppers);
		if (stprs != NULL)
		{
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			cJSON * stp = NULL;
			cJSON_ArrayForEach(stp,stprs)
			{
				Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp,key_stepperid)->valueint);
				hdata[s]->homeTimeout=getJsonInt(stp,key_home_timeout);
				hdata[s]->homeSpeed = getJsonInt(stp,key_home_speed);
				hdata[s]->homeMaxspeed =getJsonInt(stp,key_home_maxspeed);
				hdata[s]->homeDirection =getJsonInt(stp,key_home_direction);
				hdata[s]->homeEndposRelease=getJsonInt(stp,key_home_endposrelease);
				hdata[s]->homeEndStopPolarity =getJsonInt(stp,key_home_endstoppolarity);

				// grab current time
				hdata[s]->homeTimeStarted = millis();
				hdata[s]->homeIsActive = true;

				// trigger go home by starting the motor in the right direction
				FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
				motor->data[s]->isforever = true;
				motor->data[s]->speed = hdata[s]->homeDirection * hdata[s]->homeSpeed;
				motor->data[s]->maxspeed = hdata[s]->homeDirection * hdata[s]->homeMaxspeed;

				// now we will go into loop and need to stop once the button is hit or timeout is reached
				log_i("Home Data Motor  Axis: %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i, homeEndosRelease %i",
					   s, hdata[s]->homeTimeout, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection, hdata[s]->homeTimeStarted, hdata[s]->homeEndposRelease);

				motor->startStepper(s);
			}
		}
	}
	return 1;
}

cJSON * HomeMotor::get(cJSON * ob)
{
	log_i("home_get_fct");
	cJSON * doc = cJSON_CreateObject();
	cJSON * home = cJSON_CreateObject();
	cJSON_AddItemToObject(doc,key_home, home);
	cJSON * arr = cJSON_CreateArray();
	cJSON_AddItemToObject(home,key_steppers, arr);

	// add the home data to the json

	for(int i =0; i < 4;i++)
	{
		cJSON * arritem = cJSON_CreateObject();
		cJSON_AddItemToArray(arr,arritem);
		setJsonInt(arritem,key_home_timeout,hdata[i]->homeTimeout);
		setJsonInt(arritem,key_home_speed,hdata[i]->homeSpeed);
		setJsonInt(arritem,key_home_maxspeed,hdata[i]->homeMaxspeed);
		setJsonInt(arritem,key_home_direction,hdata[i]->homeDirection);
		setJsonInt(arritem,key_home_timestarted,hdata[i]->homeTimeStarted);
		setJsonInt(arritem,key_home_isactive,hdata[i]->homeIsActive);
	}

	log_i("home_get_fct done");
	return doc;
}

void sendHomeDone(int axis)
{
	// send home done to client
	cJSON * json = cJSON_CreateObject();
	cJSON * home = cJSON_CreateObject();
	cJSON_AddItemToObject(json, key_home, home);
	cJSON * steppers = cJSON_CreateObject();
	cJSON_AddItemToObject(home, key_steppers, steppers);
	cJSON * axs = cJSON_CreateNumber(axis);
	cJSON * done = cJSON_CreateNumber(true);
	cJSON_AddItemToObject(steppers, "axis", axs);
	cJSON_AddItemToObject(steppers, "isDone", done);
	Serial.println("++");
	char * ret = cJSON_Print(json);
	cJSON_Delete(json);
	Serial.println(ret);
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
