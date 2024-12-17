#include "HeatController.h"
#include "JsonKeys.h"
#include "cJsonTool.h"
#include "PinConfig.h"
#include "Arduino.h"

namespace HeatController
{

	void loop()
	{
#ifdef LASER_CONTROLLER && DS18B20_CONTROLLER 
		int pwmPin = pinConfig.LASER_0;
		if (pwmPin < 0)
		{
			return;
		}

		// we need a protection against temperature runaway and overshoot
		// get modules
		int pwmChannel = LaserController::PWM_CHANNEL_LASER_0;

		if (Heat_active && ((millis() - startMillis) >= temp_pid_updaterate))
		{

			// get rid of noise?
			float temperatureValueAvg = DS18b20Controller::getCurrentValueCelcius();
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
			if (abs(temp_pid_target - temperatureValueAvg) > 1 and (millis() - t_tempControlStarted) > hTimeout and
				hTimeout > 0)
			{
				Heat_active = false;
				pwmValue = 0;
				log_e("HeatController::loop: temperature runaway detected, stopping heat");
				hPreferences.begin("heat", false);
				hPreferences.putBool("heatactive", Heat_active);
				hPreferences.end();
			}
			log_i("Heat: %f, pwmValue: %f, pwmChannel: %i", temperatureValueAvg, pwmValue, pwmChannel);
			LaserController::setPWM(pwmValue, pwmChannel);
			startMillis = millis();
			Heat_was_active = true;
		}
		else if (Heat_active == false and Heat_was_active == true)
		{ // if we are not using it, turn it off - but only once! 
			if (ledcRead(pwmChannel) != 0)
			{
				log_i("Heat: %f, pwmValue: %f", DS18b20Controller::getCurrentValueCelcius(), 0.0f);
				LaserController::setPWM(0, pwmChannel);
			}
			Heat_was_active = false;
		}

#endif
	}

	long returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd)
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

		// 	if (isDEBUG)
		// log_d("analoginValue: " + String(analoginValue) + ", P: " + String(cP) + ", I: " + String(cI) + ", D: " + String(cD) + ", errorRunSum: " + String(errorRunSum) + ", previousError: " + String(previousError) + ", stepperOut: " + String(stepperOut));
		return stepperOut;
	}

	// Custom function accessible by the API
	int act(cJSON *ob)
	{
		int qid = cJsonTool::getJsonInt(ob, "qid");
#ifdef LASER_CONTROLLER && DS18B20_CONTROLLER
		hPreferences.begin("heat", false);
		// {"task": "/heat_act", "active":1, "Kp":1000, "Ki":0.1, "Kd":0.1, "target":22, "timeout":600000, "updaterate":1000, "qid":1}
		// {"task": "/heat_act", "active":0}
		// if json has key "active" then we set the heat active or not
		if (cJSON_HasObjectItem(ob, key_heatactive))
		{
			Heat_active = cJsonTool::getJsonInt(ob, key_heatactive);
			hPreferences.putBool("heatactive", Heat_active);
		}
		else
		{
			Heat_active = false;
		}
		// if json has key "Kp" then we set the heat active or not
		if (cJSON_HasObjectItem(ob, key_Kp))
		{
			temp_pid_Kp = cJsonTool::getJsonInt(ob, key_Kp);
			hPreferences.putFloat("temp_pid_Kp", temp_pid_Kp);
		}
		else
		{
			temp_pid_Kp = 10;
		}
		// if json has key "Ki" then we set the heat active or not
		if (cJSON_HasObjectItem(ob, key_Ki))
		{
			temp_pid_Ki = cJsonTool::getJsonInt(ob, key_Ki);
			hPreferences.putFloat("temp_pid_Ki", temp_pid_Ki);
		}
		else
		{
			temp_pid_Ki = 0.1;
		}
		// if json has key "Kd" then we set the heat active or not
		if (cJSON_HasObjectItem(ob, key_Kd))
		{
			temp_pid_Kd = cJsonTool::getJsonInt(ob, key_Kd);
			hPreferences.putFloat("temp_pid_Kd", temp_pid_Kd);
		}
		else
		{
			temp_pid_Kd = 0.1;
		}
		// if json has key "target" then we set the heat active or not
		if (cJSON_HasObjectItem(ob, key_target))
		{
			temp_pid_target = cJsonTool::getJsonInt(ob, key_target);
			hPreferences.putFloat("temp_pid_target", temp_pid_target);
		}
		else
		{
			Heat_active = false;
			return 0;
		}
		// if json has key "updaterate" then we set the heat active or not
		if (cJSON_HasObjectItem(ob, key_heat_updaterate))
		{
			temp_pid_updaterate = cJsonTool::getJsonInt(ob, key_heat_updaterate);
			hPreferences.putFloat("temp_pid_updaterate", temp_pid_updaterate);
		}
		else
		{
			temp_pid_updaterate = 1000;
		}

		// if json has key "hTimeout" then we set the heat active or not
		if (cJSON_HasObjectItem(ob, key_hTimeout))
		{
			hTimeout = cJsonTool::getJsonInt(ob, key_hTimeout);
			hPreferences.putInt("hTimeout", hTimeout);
		}
		else
		{
			hTimeout = -1;
			hPreferences.putInt("hTimeout", hTimeout);
		}

		// some safety mechanisms
		t_tempControlStarted = millis();
		temp_tempControlStarted = DS18b20Controller::getCurrentValueCelcius();
		hPreferences.putLong("t_tempControlStarted", t_tempControlStarted);
		hPreferences.putFloat("temp_tempControlStarted", temp_tempControlStarted);
		log_d("Heat_act_fct");
		log_i("Heat_active: %d, temp_pid_Kp: %f, temp_pid_Ki: %f, temp_pid_Kd: %f, temp_pid_target: %f, temp_pid_updaterate: %f", Heat_active, temp_pid_Kp, temp_pid_Ki, temp_pid_Kd, temp_pid_target, temp_pid_updaterate);
		hPreferences.end();
#endif
		return qid;
	}

	// Custom function accessible by the API
	cJSON *get(cJSON *ob)
	{
		cJSON *ret = cJSON_CreateObject();
#ifdef LASER_CONTROLLER && DS18B20_CONTROLLER		
		// {"task": "/heat_get"}
		float temperatureValueAvg = 9999.0f;
		temperatureValueAvg = DS18b20Controller::getCurrentValueCelcius();
		cJsonTool::setJsonFloat(ret, key_heat, temperatureValueAvg);
#endif
		return ret;
	}

	void setup()
	{
#ifdef LASER_CONTROLLER and DS18B20_CONTROLLER
		log_d("Setup Heat");
		startMillis = millis();

		// load hPreferences
		hPreferences.begin("heat", false);
		Heat_active = hPreferences.getBool("heatactive", false);
		temp_pid_Kp = hPreferences.getFloat("temp_pid_Kp", 10);
		temp_pid_Ki = hPreferences.getFloat("temp_pid_Ki", 0.1);
		temp_pid_Kd = hPreferences.getFloat("temp_pid_Kd", 0.1);
		temp_pid_target = hPreferences.getFloat("temp_pid_target", 37);
		temp_pid_updaterate = hPreferences.getFloat("temp_pid_updaterate", 1000);
		hTimeout = hPreferences.getInt("hTimeout", -1);
		log_i("Heat_active: %d, temp_pid_Kp: %f, temp_pid_Ki: %f, temp_pid_Kd: %f, temp_pid_target: %f, temp_pid_updaterate: %f", Heat_active, temp_pid_Kp, temp_pid_Ki, temp_pid_Kd, temp_pid_target, temp_pid_updaterate);
		if (Heat_active)
		{
			// some safety mechanisms
			t_tempControlStarted = millis();
			temp_tempControlStarted = DS18b20Controller::getCurrentValueCelcius();			
		}
		hPreferences.end();
	}
#endif
}