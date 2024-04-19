#include "HeatController.h"

HeatController::HeatController(/* args */){};
HeatController::~HeatController(){};

// Custom function accessible by the API
int HeatController::act(cJSON *ob)
{
	if (moduleController.get(AvailableModules::laser) != nullptr and moduleController.get(AvailableModules::ds18b20) != nullptr)
	{
		preferences.begin("heat", false);
		// {"task": "/heat_act", "active":1, "Kp":1000, "Ki":0.1, "Kd":0.1, "target":37, "timeout":600000, "updaterate":1000}
		// {"task": "/heat_act", "active":0}
		// if json has key "active" then we set the heat active or not
		if(cJSON_HasObjectItem(ob, key_heatactive)){
			Heat_active = getJsonInt(ob, key_heatactive);
			preferences.putBool("heatactive", Heat_active);
		}
		else{
			Heat_active = false;
		}
		// if json has key "Kp" then we set the heat active or not
		if(cJSON_HasObjectItem(ob, key_Kp)){
			temp_pid_Kp = getJsonInt(ob, key_Kp);
			preferences.putFloat("temp_pid_Kp", temp_pid_Kp);
		}
		else{
			temp_pid_Kp = 10;
		}
		// if json has key "Ki" then we set the heat active or not
		if(cJSON_HasObjectItem(ob, key_Ki)){
			temp_pid_Ki = getJsonInt(ob, key_Ki);
			preferences.putFloat("temp_pid_Ki", temp_pid_Ki);
		}
		else{
			temp_pid_Ki = 0.1;
		}
		// if json has key "Kd" then we set the heat active or not
		if(cJSON_HasObjectItem(ob, key_Kd)){
			temp_pid_Kd = getJsonInt(ob, key_Kd);
			preferences.putFloat("temp_pid_Kd", temp_pid_Kd);
		}
		else{
			temp_pid_Kd = 0.1;
		}
		// if json has key "target" then we set the heat active or not
		if(cJSON_HasObjectItem(ob, key_target)){
			temp_pid_target = getJsonInt(ob, key_target);
			preferences.putFloat("temp_pid_target", temp_pid_target);
		}
		else{
			Heat_active = false;
			return 0;
		}
		// if json has key "updaterate" then we set the heat active or not
		if(cJSON_HasObjectItem(ob, key_heat_updaterate)){
			temp_pid_updaterate = getJsonInt(ob, key_heat_updaterate);
			preferences.putFloat("temp_pid_updaterate", temp_pid_updaterate);
		}
		else{
			temp_pid_updaterate = 1000;
		}
		
		// if json has key "timeToReach80PercentTargetTemperature" then we set the heat active or not
		if(cJSON_HasObjectItem(ob, key_timeToReach80PercentTargetTemperature)){
			timeToReach80PercentTargetTemperature = getJsonInt(ob, key_timeToReach80PercentTargetTemperature);
			preferences.putInt("timeToReach80PercentTargetTemperature", timeToReach80PercentTargetTemperature);
		}
		else{
			timeToReach80PercentTargetTemperature = -1;
			preferences.putInt("timeToReach80PercentTargetTemperature", timeToReach80PercentTargetTemperature);
		}
		

		// some safety mechanisms
		t_tempControlStarted = millis();
		DS18b20Controller *ds18b20 = (DS18b20Controller *)moduleController.get(AvailableModules::ds18b20);
		temp_tempControlStarted = ds18b20->currentValueCelcius;
		preferences.putLong("t_tempControlStarted", t_tempControlStarted);
		preferences.putFloat("temp_tempControlStarted", temp_tempControlStarted);
		log_d("Heat_act_fct");
		log_i("Heat_active: %d, temp_pid_Kp: %f, temp_pid_Ki: %f, temp_pid_Kd: %f, temp_pid_target: %f, temp_pid_updaterate: %f", Heat_active, temp_pid_Kp, temp_pid_Ki, temp_pid_Kd, temp_pid_target, temp_pid_updaterate);
		preferences.end();
		return 1;
	}
	else
	{
		log_e("HeatController::act: no laser or ds18b20 module found");
		return 0;
	}
}

void HeatController::loop()
{
	int pwmPin = pinConfig.LASER_0;
	if (pwmPin < 0)
	{
		return;
	}
	
	// we need a protection against temperature runaway and overshoot

	if (moduleController.get(AvailableModules::laser) != nullptr and moduleController.get(AvailableModules::ds18b20) != nullptr)
	{
		// get modules
		LaserController *pwmController = (LaserController *)moduleController.get(AvailableModules::laser);
		DS18b20Controller *ds18b20 = (DS18b20Controller *)moduleController.get(AvailableModules::ds18b20);

		int pwmChannel = pwmController->PWM_CHANNEL_LASER_0;

		if (Heat_active && ((millis() - startMillis) >= temp_pid_updaterate))
		{

			// get rid of noise?
			float temperatureValueAvg = ds18b20->currentValueCelcius;
			float pwmValue = 0;
			if (temperatureValueAvg < 60)
			{
				pwmValue = returnControlValue(temp_pid_target, temperatureValueAvg, temp_pid_Kp, temp_pid_Ki, temp_pid_Kd);
				if (pwmValue > maxPWMValue)
				{
					pwmValue = maxPWMValue;
				}
			}

			// some safety mechanisms
			// if  temperature is not in a certain band after a time X, we have a runaway
			if (abs(temp_pid_target-temperatureValueAvg)>1 and (millis() - t_tempControlStarted) > timeToReach80PercentTargetTemperature and
				timeToReach80PercentTargetTemperature>0)	
			{
				Heat_active = false;
				pwmValue = 0;
				log_e("HeatController::loop: temperature runaway detected, stopping heat");
			}
			log_i("Heat: %f, pwmValue: %f", temperatureValueAvg, pwmValue);
			pwmController->setPWM(pwmValue, pwmChannel);
			startMillis = millis();
		}
		else if (Heat_active == false)
		{ // if we are not using it, turn it off
		if (ledcRead(pwmChannel)!=0){
			log_i("Heat: %f, pwmValue: %f", ds18b20->currentValueCelcius, 0.0f);
			pwmController->setPWM(0, pwmChannel);
		}
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
	// {"task": "/heat_get"}
	if (DEBUG)
		log_d("Heat_get_fct");
	
	int qid = getJsonInt(ob, "qid");
	cJSON *ret = cJSON_CreateObject();
	float temperatureValueAvg = 9999.0f;
	if (moduleController.get(AvailableModules::laser) != nullptr and moduleController.get(AvailableModules::ds18b20) != nullptr)
	{
		// get modules
		DS18b20Controller *ds18b20 = (DS18b20Controller *)moduleController.get(AvailableModules::ds18b20);
		temperatureValueAvg = ds18b20->currentValueCelcius;
	}
	setJsonFloat(ret, key_heat, temperatureValueAvg);
	cJSON_AddItemToObject(ret, keyQueueID, cJSON_CreateNumber(qid));

	return ret;
}

void HeatController::setup()
{
	log_d("Setup Heat");
	startMillis = millis();

	// load preferences
	preferences.begin("heat", false);
	Heat_active = preferences.getBool("heatactive", false);
	temp_pid_Kp = preferences.getFloat("temp_pid_Kp", 10);
	temp_pid_Ki = preferences.getFloat("temp_pid_Ki", 0.1);
	temp_pid_Kd = preferences.getFloat("temp_pid_Kd", 0.1);
	temp_pid_target = preferences.getFloat("temp_pid_target", 37);
	temp_pid_updaterate = preferences.getFloat("temp_pid_updaterate", 1000);
	timeToReach80PercentTargetTemperature = preferences.getInt("timeToReach80PercentTargetTemperature", -1);
	log_i("Heat_active: %d, temp_pid_Kp: %f, temp_pid_Ki: %f, temp_pid_Kd: %f, temp_pid_target: %f, temp_pid_updaterate: %f", Heat_active, temp_pid_Kp, temp_pid_Ki, temp_pid_Kd, temp_pid_target, temp_pid_updaterate);
	if (Heat_active){
		// some safety mechanisms
		t_tempControlStarted = millis();
		DS18b20Controller *ds18b20 = (DS18b20Controller *)moduleController.get(AvailableModules::ds18b20);
		temp_tempControlStarted = ds18b20->currentValueCelcius;
	}
	preferences.end();
}
