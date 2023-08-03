#include "PidController.h"

PidController::PidController(/* args */){};
PidController::~PidController(){};

// Custom function accessible by the API
int PidController::act(cJSON *ob)
{

	PID_active = getJsonInt(ob, key_PIDactive);
	PID_Kp = getJsonInt(ob, key_Kp);
	PID_Ki = getJsonInt(ob, key_Ki);
	PID_Kd = getJsonInt(ob, key_Kd);
	PID_target = getJsonInt(ob, key_target);
	PID_updaterate = getJsonInt(ob, key_PID_updaterate);
	// here you can do something
	if (DEBUG)
		Serial.println("PID_act_fct");

	if (!PID_active)
	{
		// force shutdown the motor
		if (moduleController.get(AvailableModules::motor) != nullptr)
		{
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			motor->data[Stepper::X]->speed = 0;
			motor->stopStepper(Stepper::X);
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
		int analoginpin = pinConfig.pid1;

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
			motor->startStepper(Stepper::X);
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

// Custom function accessible by the API
cJSON *PidController::get(cJSON *ob)
{
	if (DEBUG)
		Serial.println("PID_get_fct");
	int PIDID = getJsonInt(ob, key_PIDID);
	int PIDPIN = 0;
	switch (PIDID)
	{
	case 0:
		PIDPIN = pinConfig.pid1;
		break;
	case 1:
		PIDPIN = pinConfig.pid2;
		break;
	case 2:
		PIDPIN = pinConfig.pid3;
		break;
	}

	cJSON *ret = cJSON_CreateObject();
	setJsonInt(ret, key_N_analogin_avg, N_analogin_avg);
	setJsonInt(ret, key_PIDPIN, PIDPIN);
	setJsonInt(ret, key_PIDID, PIDID);
	return ret;
}

void PidController::setup()
{
	log_d("Setup PID");
	startMillis = millis();
}
