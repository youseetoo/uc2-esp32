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
		FocusMotor * motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		DigitalInController * digitalin = (DigitalInController*)moduleController.get(AvailableModules::digitalin);
		//expecting digitalin1 handling endstep for stepper X, digital2 stepper Y, digital3 stepper Z
		if(digitalin->digitalin_val_1)
		{
			int speed = motor->data[Stepper::X]->speed;
			motor->steppers[Stepper::X]->stop();
			//blocks until stepper reached new position wich would be optimal outside of the endstep
			if(speed > 0)
				motor->steppers[Stepper::X]->runToNewPosition(-100);
			else
				motor->steppers[Stepper::X]->runToNewPosition(100);
		}
		if(digitalin->digitalin_val_2)
		{
			int speed = motor->data[Stepper::Y]->speed;
			motor->steppers[Stepper::X]->stop();
			if(speed > 0)
				motor->steppers[Stepper::Y]->runToNewPosition(-100);
			else
				motor->steppers[Stepper::Y]->runToNewPosition(100);
		}
		if(digitalin->digitalin_val_3)
		{
			int speed = motor->data[Stepper::Z]->speed;
			motor->steppers[Stepper::Z]->stop();
			if(speed > 0)
				motor->steppers[Stepper::Z]->runToNewPosition(-100);
			else
				motor->steppers[Stepper::Z]->runToNewPosition(100);
		}
	}
}
/*
not needed all stuff get setup inside motor and digitalin, but must get implemented
*/
void HomeMotor::setup()
{}