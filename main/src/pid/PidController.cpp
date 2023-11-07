#include "PidController.h"
#include "cJsonTool.h"
#include "Arduino.h"
#include "PinConfig.h"
#include "JsonKeys.h"

namespace PidController
{
	// Custom function accessible by the API
	int act(cJSON *ob)
	{

		PID_active = cJsonTool::getJsonInt(ob, key_PIDactive);
		PID_Kp = cJsonTool::getJsonInt(ob, key_Kp);
		PID_Ki = cJsonTool::getJsonInt(ob, key_Ki);
		PID_Kd = cJsonTool::getJsonInt(ob, key_Kd);
		PID_target = cJsonTool::getJsonInt(ob, key_target);
		PID_updaterate = cJsonTool::getJsonInt(ob, key_PID_updaterate);
		// here you can do something
		if (DEBUG)
			log_d("PID_act_fct");

		if (!PID_active)
		{
// force shutdown the motor
#ifdef FOCUS_MOTOR
			FocusMotor::data[Stepper::X]->speed = 0;
			FocusMotor::stopStepper(Stepper::X);
#endif
		}
		return 1;
	}

	void loop()
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
#ifdef FOCUS_MOTOR
			FocusMotor::data[Stepper::X]->isforever = 1; // run motor at certain speed
			FocusMotor::data[Stepper::X]->speed = motorValue;
			FocusMotor::startStepper(Stepper::X);
#endif
			startMillis = millis();
		}
	}

	long returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd)
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
			log_d("analoginValue: " + String(analoginValue) + ", P: " + String(cP) + ", I: " + String(cI) + ", D: " + String(cD) + ", errorRunSum: " + String(errorRunSum) + ", previousError: " + String(previousError) + ", stepperOut: " + String(stepperOut));
		return stepperOut;
	}

	// Custom function accessible by the API
	// returns json {"pid":{...}} as qid
	cJSON *get(cJSON *ob)
	{
		if (DEBUG)
			log_d("PID_get_fct");
		int PIDID = cJsonTool::getJsonInt(ob, key_PIDID);
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
		cJSON *pd = cJSON_CreateObject();
		cJSON_AddItemToObject(ret, key_pid, pd);
		cJsonTool::setJsonInt(pd, key_N_analogin_avg, N_analogin_avg);
		cJsonTool::setJsonInt(pd, key_PIDPIN, PIDPIN);
		cJsonTool::setJsonInt(pd, key_PIDID, PIDID);
		return ret;
	}

	void setup()
	{
		log_d("Setup PID");
		startMillis = millis();
	}
}
