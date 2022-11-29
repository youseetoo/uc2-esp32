#include "../../config.h"
#include "PidController.h"

namespace RestApi
{
	void Pid_act()
	{
		serialize(moduleController.get(AvailableModules::pid)->act(deserialize()));
	}

	void Pid_get()
	{
		serialize(moduleController.get(AvailableModules::pid)->get(deserialize()));
	}

	void Pid_set()
	{
		serialize(moduleController.get(AvailableModules::pid)->set(deserialize()));
	}
}

PidController::PidController(/* args */){};
PidController::~PidController(){};

// Custom function accessible by the API
int PidController::act(DynamicJsonDocument ob)
{

	// here you can do something
	if (DEBUG)
		Serial.println("PID_act_fct");

	if (ob.containsKey(key_PIDactive))
		PID_active = (int)(ob)[key_PIDactive];
	if (ob.containsKey(key_Kp))
		PID_Kp = (ob)[key_Kp];
	if ((ob).containsKey(key_Ki))
		PID_Ki = (ob)[key_Ki];
	if (ob.containsKey(key_Kd))
		PID_Kd = (ob)[key_Kd];
	if (ob.containsKey(key_target))
		PID_target = (ob)[key_target];
	if (ob.containsKey(key_PID_updaterate))
		PID_updaterate = (int)(ob)[key_PID_updaterate];

	if (!PID_active)
	{
		// force shutdown the motor
		if (moduleController.get(AvailableModules::motor) != nullptr)
		{
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			motor->data[Stepper::X]->speed = 0;
			motor->steppers[Stepper::X]->setSpeed(0);
			motor->steppers[Stepper::X]->setMaxSpeed(0);
			motor->steppers[Stepper::X]->runSpeed();
		}
	}
	return 1;
}

void PidController::loop()
{
	currentMillis = millis();
	if (PID_active && (currentMillis - startMillis >= PID_updaterate))
	{
		// hardcoded for now:
		int N_analogin_avg = 50;
		int analoginpin = pins.analogin_PIN_0;

		// get rid of noise?
		float analoginValueAvg = 0;
		for (int imeas = 0; imeas < N_analogin_avg; imeas++)
		{
			analoginValueAvg += analogRead(analoginpin);
		}

		analoginValueAvg = (float)analoginValueAvg / (float)N_analogin_avg;
		long motorValue = returnControlValue(PID_target, analoginValueAvg, PID_Kp, PID_Ki, PID_Kd);
		if (moduleController.get(AvailableModules::motor) != nullptr)
		{
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			motor->data[Stepper::X]->isforever = 1; // run motor at certain speed
			motor->data[Stepper::X]->speed = motorValue;
			motor->steppers[Stepper::X]->setSpeed(motorValue);
			motor->steppers[Stepper::X]->setMaxSpeed(motorValue);
		}
		startMillis = millis();
	}
}

long PidController::returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd)
{
	float analoginOffset = 0.;
	float maxError = 1.;
	float error = (controlTarget - (analoginValue - analoginOffset)) / maxError;
	float cP = Kp * error;
	float cI = Ki * errorRunSum;
	float cD = Kd * (error - previousError);
	float PID = cP + cI + cD;
	long stepperOut = (long)PID;

	if (stepperOut > stepperMaxValue)
	{
		stepperOut = stepperMaxValue;
	}

	if (stepperOut < -stepperMaxValue)
	{
		stepperOut = -stepperMaxValue;
	}

	errorRunSum = errorRunSum + error;
	previousError = error;

	if (DEBUG)
		Serial.println("analoginValue: " + String(analoginValue) + ", P: " + String(cP) + ", I: " + String(cI) + ", D: " + String(cD) + ", errorRunSum: " + String(errorRunSum) + ", previousError: " + String(previousError) + ", stepperOut: " + String(stepperOut));
	return stepperOut;
}

int PidController::set(DynamicJsonDocument ob)
{
	if (DEBUG)
		Serial.println("PID_set_fct");
	int PIDID = (int)(ob)[key_PIDID];
	int PIDPIN = (int)(ob)[key_PIDPIN];
	if (ob.containsKey(key_N_analogin_avg))
		N_analogin_avg = (int)(ob)[key_N_analogin_avg];

	switch (PIDID)
	{
	case 0:
		pins.analogin_PIN_0 = PIDPIN;
		break;
	case 1:
		pins.analogin_PIN_1 = PIDPIN;
		break;
	case 2:
		pins.analogin_PIN_2 = PIDPIN;
		break;
	}
	return 1;
}

// Custom function accessible by the API
DynamicJsonDocument PidController::get(DynamicJsonDocument ob)
{
	if (DEBUG)
		Serial.println("PID_get_fct");
	int PIDID = (int)(ob)[key_PIDID];
	int PIDPIN = 0;
	switch (PIDID)
	{
	case 0:
		PIDPIN = pins.analogin_PIN_0;
		break;
	case 1:
		PIDPIN = pins.analogin_PIN_1;
		break;
	case 2:
		PIDPIN = pins.analogin_PIN_2;
		break;
	}

	ob.clear();
	ob[N_analogin_avg] = N_analogin_avg;
	ob[key_PIDPIN] = PIDPIN;
	ob[key_PIDID] = PIDID;
	return ob;
}

void PidController::setup()
{
	startMillis = millis();
	if (DEBUG)
		Serial.println("Setting up analogins...");
}
