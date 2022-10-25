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


// {"task":"/home_act", "home": {"home":1, "steppers": [{"id":0, "endpospin": 0, "timeout": 10000, "speed": 1000, "direction":1}]}}
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
			Serial.println("1");
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_endpospin))
				hdata[s]->homeEndposPin = (*j)[key_home][key_steppers][i][key_home_endpospin];
			Serial.println("2");
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_timeout))
				hdata[s]->homeTimeout = (*j)[key_home][key_steppers][i][key_home_timeout];
			Serial.println("3");
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_speed))
				hdata[s]->homeSpeed = (*j)[key_home][key_steppers][i][key_home_speed];
			Serial.println("4");
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_maxspeed))
				hdata[s]->homeMaxspeed = (*j)[key_home][key_steppers][i][key_home_maxspeed];
			Serial.println("5");
			if ((*j)[key_home][key_steppers][i].containsKey(key_home_direction))
				hdata[s]->homeDirection = (*j)[key_home][key_steppers][i][key_home_direction];
			Serial.println("6");
			doHome(s);
	
	
		}
	}
	}

	WifiController::getJDoc()->clear();
}


void HomeMotor::doHome(int i){
	Serial.println("do Home");
		//if (moduleController.get(AvailableModules::motor) != nullptr)
		//if (moduleController.get(AvailableModules::digitalout) != nullptr)
		//if (moduleController.get(AvailableModules::digitalin) != nullptr)

	log_i("home stepper:%i direction:%i, endpos Pin: %i", i, hdata[i]->homeDirection, hdata[i]->homeEndposPin);
	
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
{}

void HomeMotor::setup()
{}