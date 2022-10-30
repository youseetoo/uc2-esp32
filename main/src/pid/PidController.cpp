#include "../../config.h"
#include "PidController.h"

namespace RestApi
{
	void Pid_act()
	{
		deserialize();
		moduleController.get(AvailableModules::pid)->act();
		serialize();
	}

	void Pid_get()
	{
		deserialize();
		moduleController.get(AvailableModules::pid)->get();
		serialize();
	}

	void Pid_set()
	{
		deserialize();
		moduleController.get(AvailableModules::pid)->set();
		serialize();
	}
}

PidController::PidController(/* args */){};
PidController::~PidController(){};

// Custom function accessible by the API
void PidController::act()
{

	// here you can do something
	if (DEBUG)
		Serial.println("PID_act_fct");

	if (WifiController::getJDoc()->containsKey("PIDactive"))
		PID_active = (int)(*WifiController::getJDoc())["PIDactive"];
	if (WifiController::getJDoc()->containsKey("Kp"))
		PID_Kp = (*WifiController::getJDoc())["Kp"];
	if ((*WifiController::getJDoc()).containsKey("Ki"))
		PID_Ki = (*WifiController::getJDoc())["Ki"];
	if (WifiController::getJDoc()->containsKey("Kd"))
		PID_Kd = (*WifiController::getJDoc())["Kd"];
	if (WifiController::getJDoc()->containsKey("target"))
		PID_target = (*WifiController::getJDoc())["target"];
	if (WifiController::getJDoc()->containsKey("PID_updaterate"))
		PID_updaterate = (int)(*WifiController::getJDoc())["PID_updaterate"];

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

	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["Kp"] = PID_Kp;
	(*WifiController::getJDoc())["Ki"] = PID_Ki;
	(*WifiController::getJDoc())["Kd"] = PID_Kd;
	(*WifiController::getJDoc())["PID_updaterate"] = PID_updaterate;
	(*WifiController::getJDoc())["PID"] = PID_active;
	(*WifiController::getJDoc())["target"] = PID_target;
}

void PidController::loop()
{
	if (PID_active && (state.currentMillis - state.startMillis >= PID_updaterate))
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
		state.startMillis = millis();
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

void PidController::set()
{
	if (DEBUG)
		Serial.println("PID_set_fct");
	int PIDID = (int)(*WifiController::getJDoc())["PIDID"];
	int PIDPIN = (int)(*WifiController::getJDoc())["PIDPIN"];
	if (WifiController::getJDoc()->containsKey("N_analogin_avg"))
		N_analogin_avg = (int)(*WifiController::getJDoc())["N_analogin_avg"];

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

	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["PIDPIN"] = PIDPIN;
	(*WifiController::getJDoc())["PIDID"] = PIDID;
}

// Custom function accessible by the API
void PidController::get()
{
	if (DEBUG)
		Serial.println("PID_get_fct");
	int PIDID = (int)(*WifiController::getJDoc())["PIDID"];
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

	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["N_analogin_avg"] = N_analogin_avg;
	(*WifiController::getJDoc())["PIDPIN"] = PIDPIN;
	(*WifiController::getJDoc())["PIDID"] = PIDID;
}

void PidController::setup()
{
	if (DEBUG)
		Serial.println("Setting up analogins...");
}
