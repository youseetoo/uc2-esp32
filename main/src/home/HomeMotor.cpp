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
	int qid = getJsonInt(j, "qid");
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
				hdata[s]->homeEndStopPolarity =getJsonInt(stp,key_home_endstoppolarity);
				hdata[s]->qid = qid;
				hdata[s]->homeInEndposReleaseMode = false;
				// grab current time
				hdata[s]->homeTimeStarted = millis();
				hdata[s]->homeIsActive = true;

				// trigger go home by starting the motor in the right direction
				FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
				motor->data[s]->isforever = true;
				motor->data[s]->speed = hdata[s]->homeDirection * hdata[s]->homeSpeed;
				motor->data[s]->maxspeed = hdata[s]->homeDirection * hdata[s]->homeMaxspeed;
				motor->startStepper(s);
				if (s==Stepper::Z and pinConfig.isDualAxisZ){
					// we may have a dual axis so we would need to start A too
					log_i("Starting A too");
					motor->data[Stepper::A]->isforever = true;
					motor->data[Stepper::A]->speed = hdata[s]->homeDirection * hdata[s]->homeSpeed;
					motor->data[Stepper::A]->maxspeed = hdata[s]->homeDirection * hdata[s]->homeMaxspeed;
					motor->startStepper(Stepper::A);
				}
				// now we will go into loop and need to stop once the button is hit or timeout is reached
				log_i("Home Data Motor  Axis: %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i, homeEndosReleaseMode %b",
					   s, hdata[s]->homeTimeout, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection, hdata[s]->homeTimeStarted, hdata[s]->homeInEndposReleaseMode);

				
			}
		}
	}
	return qid; 
}

cJSON * HomeMotor::get(cJSON * ob)
{
	log_i("home_get_fct");
	int qid = getJsonInt(ob, "qid");
	cJSON * doc = cJSON_CreateObject();
	cJSON * home = cJSON_CreateObject();
	cJSON_AddItemToObject(doc,key_home, home);
	cJSON_AddItemToObject(doc, keyQueueID, cJSON_CreateNumber(qid));
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

void HomeMotor::sendHomeDone(int axis)
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
	cJSON_AddItemToObject(json, keyQueueID, cJSON_CreateNumber(hdata[axis]->qid));
	setJsonInt(json, keyQueueID, hdata[axis]->qid);
	Serial.println("++");
	char * ret = cJSON_PrintUnformatted(json);
	cJSON_Delete(json);
	Serial.println(ret);
	free(ret);
	Serial.println("--");
}


void HomeMotor::checkAndProcessHome(Stepper s, int digitalin_val, FocusMotor *motor)
{
	if (hdata[s]->homeIsActive && (abs(hdata[s]->homeEndStopPolarity-digitalin_val) || hdata[s]->homeTimeStarted + hdata[s]->homeTimeout < millis()) 
		&& !hdata[s]->homeInEndposReleaseMode)
		{
			// reverse direction to release endstops
			log_i("Home Motor %i hit endstop, reversing direction",s);
			motor->data[s]->speed = -motor->data[s]->speed;
			motor->startStepper(s);
			if(s==Stepper::Z and pinConfig.isDualAxisZ){
				// we may have a dual axis so we would need to start A too
				motor->data[Stepper::A]->speed = -motor->data[Stepper::A]->speed;
				motor->startStepper(Stepper::A);
			}
			hdata[s]->homeInEndposReleaseMode = true;
		}
	else if (hdata[s]->homeIsActive && hdata[s]->homeInEndposReleaseMode && 
		(!abs(hdata[s]->homeEndStopPolarity-digitalin_val) || hdata[s]->homeTimeStarted + hdata[s]->homeTimeout < millis())){
			log_i("Home Motor %i lef endstop, stopping",s);
			motor->stopStepper(s);
			motor->setPosition(s, 0);
			if (s==Stepper::Z and pinConfig.isDualAxisZ){
				// we may have a dual axis so we would need to start A too
				motor->stopStepper(Stepper::A);
				motor->setPosition(Stepper::A, 0);
			}
			motor->data[s]->isforever = false;
			log_i("Home Motor X done");
			sendHomeDone(s);
			if (s==Stepper::A  and pinConfig.isDualAxisZ){
				// we may have a dual axis so we would need to start A too
				hdata[Stepper::A]->homeIsActive = false;
				motor->setPosition(Stepper::A, 0);
				motor->stopStepper(Stepper::A);
				motor->data[Stepper::A]->isforever = false;
				log_i("Home Motor A done");
				sendHomeDone(Stepper::A);
			}
			hdata[s]->homeIsActive = false;
			hdata[s]->homeInEndposReleaseMode = false;
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
