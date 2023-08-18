#include "HomeMotor.h"
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
	free(ret);
	Serial.println("--");
}


void HomeMotor::checkAndProcessHome(Stepper s, int digitalin_val,FocusMotor *motor)
{
	if (hdata[s]->homeIsActive && (abs(hdata[s]->homeEndStopPolarity-digitalin_val) || hdata[s]->homeTimeStarted + hdata[s]->homeTimeout < millis()))
		{
			// stopping motor and going reversing direction to release endstops
			int speed = motor->data[s]->speed;
			motor->stopStepper(s);
			// blocks until stepper reached new position wich would be optimal outside of the endstep
			if (speed > 0)
				motor->data[s]->targetPosition = -hdata[s]->homeEndposRelease;
			else
				motor->data[s]->targetPosition = hdata[s]->homeEndposRelease;
			motor->startStepper(s);
			// wait until stepper reached new position
			while (motor->data[s]->currentPosition - motor->data[s]->targetPosition) delay(1);
			hdata[s]->homeIsActive = false;
			motor->setPosition(s, 0);
			motor->stopStepper(s);
			motor->data[s]->isforever = false;
			log_i("Home Motor X done");
			sendHomeDone(s);
		}
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
		checkAndProcessHome(Stepper::X, digitalin->digitalin_val_1,motor);
		checkAndProcessHome(Stepper::Y, digitalin->digitalin_val_2,motor);
		checkAndProcessHome(Stepper::Z, digitalin->digitalin_val_3,motor);
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
