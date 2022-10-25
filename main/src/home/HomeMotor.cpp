#include "../../config.h"
#include "HomeMotor.h"


HomeMotor::HomeMotor() : Module() { log_i("ctor"); }
HomeMotor::~HomeMotor() { log_i("~ctor"); }

/*
not needed but must get implemented
*/
void HomeMotor::act()
{
}

/*
not needed but must get implemented
*/
void HomeMotor::get()
{
	
}
void HomeMotor::set()
{
	
}

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
		if (digitalin->digitalin_val_1 || digitalin->digitalin_val_2 || digitalin->digitalin_val_3)
		{
			motor->stopAllDrives();
		}
	}
}
/*
not needed all stuff get setup inside motor and digitalin, but must get implemented
*/
void HomeMotor::setup()
{}