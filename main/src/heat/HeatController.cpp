#include "HeatController.h"

HeatController::HeatController(/* args */){};
HeatController::~HeatController(){};

// Custom function accessible by the API
int HeatController::act(cJSON *ob)
{
// {"task": "/heat_act", "active":1, "Kp":100, "Ki":0.1, "Kd":0.1, "target":30, "updaterate":1000}
// {"task": "/heat_act", "active":0}
	Heat_active = getJsonInt(ob, key_heatactive);
	temp_pid_Kp = getJsonInt(ob, key_Kp);
	temp_pid_Ki = getJsonInt(ob, key_Ki);
	temp_pid_Kd = getJsonInt(ob, key_Kd);
	temp_pid_target = getJsonInt(ob, key_target);
	temp_pid_updaterate = getJsonInt(ob, key_heat_updaterate);
	log_d("Heat_act_fct");
	log_i("Heat_active: %d, temp_pid_Kp: %f, temp_pid_Ki: %f, temp_pid_Kd: %f, temp_pid_target: %f, temp_pid_updaterate: %f", Heat_active, temp_pid_Kp, temp_pid_Ki, temp_pid_Kd, temp_pid_target, temp_pid_updaterate);

	return 1;
}

void HeatController::loop()
{
	if (Heat_active && ((millis() - startMillis) >= temp_pid_updaterate))
	{
		if (moduleController.get(AvailableModules::laser) != nullptr and moduleController.get(AvailableModules::ds18b20) != nullptr)
		{
			LaserController *pwmController = (LaserController *)moduleController.get(AvailableModules::laser);
			DS18b20Controller *ds18b20 = (DS18b20Controller *)moduleController.get(AvailableModules::ds18b20);


			// get rid of noise?
			float temperatureValueAvg = ds18b20->currentValueCelcius;
			int pwmChannel = 1;
			float pwmValue = returnControlValue(temp_pid_target, temperatureValueAvg, temp_pid_Kp, temp_pid_Ki, temp_pid_Kd);
			log_i("Heat: %f, pwmValue: %f", temperatureValueAvg, pwmValue);
		#
			pwmController->setPWM(pwmValue, pwmChannel);
			startMillis = millis();
		}
	}
}

long HeatController::returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd)
{
	float analoginOffset = 0.;
	float maxError = 1.;
	float error = (controlTarget - (analoginValue - analoginOffset)) / maxError;
	float cP = Kp * error;
	float cI = Ki * errorRunSum;
	float cD = Kd * (error - previousError);
	float Heat = cP + cI + cD;
	long stepperOut = (long)Heat;

	if (stepperOut > pwm_max_value)
	{
		stepperOut = pwm_max_value;
	}

	if (stepperOut < pwm_min_value)
	{
		stepperOut = pwm_min_value;
	}

	errorRunSum = errorRunSum + error;
	previousError = error;

	// 	if (DEBUG)
	// log_d("analoginValue: " + String(analoginValue) + ", P: " + String(cP) + ", I: " + String(cI) + ", D: " + String(cD) + ", errorRunSum: " + String(errorRunSum) + ", previousError: " + String(previousError) + ", stepperOut: " + String(stepperOut));
	return stepperOut;
}

// Custom function accessible by the API
cJSON *HeatController::get(cJSON *ob)
{
	if (DEBUG)
		log_d("Heat_get_fct");


	cJSON *ret = cJSON_CreateObject();
	setJsonInt(ret, key_N_analogin_avg, N_analogin_avg);
	/*
	setJsonInt(ret, key_heatPIN, HeatPIN);
	setJsonInt(ret, key_heatID, HeatID);
	*/
	return ret;
}

void HeatController::setup()
{
	log_d("Setup Heat");
	startMillis = millis();
}
