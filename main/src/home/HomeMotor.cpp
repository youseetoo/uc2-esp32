#include "../../config.h"
#include "HomeMotor.h"



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

HomeMotor::HomeMotor() : Module() { log_i("ctor"); }
HomeMotor::~HomeMotor() { log_i("~ctor"); }


// {"task":"/home_act", "home": {"home":1, "steppers": [{"id":1, "endpospin": 0, "timeout": 10000, "speed": 1000, "direction":1}]}}
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
				hdata[s]->homeTimeout = (*j)[key_home][key_steppers][i][key_home_timeout];
			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_speed))
				hdata[s]->homeSpeed = (*j)[key_home][key_steppers][i][key_home_speed];
			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_maxspeed))
				hdata[s]->homeMaxspeed = (*j)[key_home][key_steppers][i][key_home_maxspeed];
			
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_direction))
				hdata[s]->homeDirection = (*j)[key_home][key_steppers][i][key_home_direction];
			
			// go home 
			doHome(s);
	
		}
	}
	}

	WifiController::getJDoc()->clear();
}


void HomeMotor::doHome(int i){
	Serial.println("do Home");
	log_i("home stepper:%i direction:%i, endpos Pin: %i, homeMaxSpeed: %i, homeSpeed: %i, homeTimeout: %i ", i, hdata[i]->homeDirection, hdata[i]->homeEndposPin, hdata[i]->homeMaxspeed, hdata[i]->homeSpeed, hdata[i]->homeTimeout);

	DigitalInController * digitalin = (DigitalInController*)moduleController.get(AvailableModules::digitalin);
	FocusMotor * motor = (FocusMotor*)moduleController.get(AvailableModules::motor);
	motor->stopAllDrives(); // Make sure nothing is moving
	if (moduleController.get(AvailableModules::motor) != nullptr)
		{
			// initiate a motor run and stop once the endstop is hit
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			motor->steppers[i]->setSpeed(hdata[i]->homeDirection*hdata[i]->homeSpeed);
			
			long cTime = millis();
			// FIXME: This is a blocking operation - not a good idea!
			bool breakCondition = false;
			while(!breakCondition){
				// FIXME: Better to put this into an array..
				// Check if limitswitch was hit
				if(hdata[i]->homeEndposPin==1){
					//breakCondition = digitalin->digitalin_val_1;
					breakCondition = digitalRead(pins.digitalin_PIN_1);
					Serial.println(pins.digitalin_PIN_1);
				}
				else if(hdata[i]->homeEndposPin==2){
					//breakCondition = digitalin->digitalin_val_2;
					breakCondition = digitalRead(pins.digitalin_PIN_2);
					Serial.println(pins.digitalin_PIN_2);
				}
				else if(hdata[i]->homeEndposPin==3){
					//breakCondition = digitalin->digitalin_val_3;
					breakCondition = digitalRead(pins.digitalin_PIN_3);
					Serial.println(pins.digitalin_PIN_3);
				}
				if (millis()-cTime>hdata[i]->homeTimeout)
					breakCondition = true;

				Serial.print("Breakcondition: ");
				Serial.println(breakCondition);

				// Run Motor forever..
				motor->steppers[i]->runSpeed();
				
			}

			// resetting the current position of this motor 
			motor->steppers[i]->setCurrentPosition(0);
			
			// alternative: run motor in background, but how to trigger a stop by a motor?
			// motor->data[i]->isforever = true;
		}
}

void HomeMotor::set()
{
	DynamicJsonDocument *doc = WifiController::getJDoc();
	if (doc->containsKey(key_home))
	{
		
		if ((*doc)[key_home].containsKey(key_steppers))
		{
			for (int i = 0; i < (*doc)[key_home][key_steppers].size(); i++)
			{
				
				}

		}

	}
	doc->clear();
}

void HomeMotor::get()
{
	DynamicJsonDocument *doc = WifiController::getJDoc();
	doc->clear();
	/*
	for (int i = 0; i < steppers.size(); i++)
	{
		(*doc)[key_steppers][i][key_stepperid] = i;
		(*doc)[key_steppers][i][key_dir] = pins[i]->DIR;
		(*doc)[key_steppers][i][key_step] = pins[i]->STEP;
		(*doc)[key_steppers][i][key_enable] = pins[i]->ENABLE;
		(*doc)[key_steppers][i][key_dir_inverted] = pins[i]->direction_inverted;
		(*doc)[key_steppers][i][key_step_inverted] = pins[i]->step_inverted;
		(*doc)[key_steppers][i][key_enable_inverted] = pins[i]->enable_inverted;
		(*doc)[key_steppers][i][key_position] = hdata[i]->currentPosition;
		(*doc)[key_steppers][i][key_speed] = hdata[i]->speed;
		(*doc)[key_steppers][i][key_speedmax] = hdata[i]->maxspeed;
		(*doc)[key_steppers][i][key_max_position] = pins[i]->max_position;
		(*doc)[key_steppers][i][key_min_position] = pins[i]->min_position;
		(*doc)[key_steppers][i][key_current_position] = pins[i]->current_position;
	}
	*/
}


void HomeMotor::loop()
{

}

void HomeMotor::setup()
{}