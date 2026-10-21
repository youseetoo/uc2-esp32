// HeatController (Phase 4: pure C)
#include "HeatController.h"
#include "JsonKeys.h"
#include "cJsonTool.h"
#include "PinConfig.h"
#include "DS18b20Controller.h"
#include "LaserController.h"
#include <uc2_hal/uc2_time.h>
#include <uc2_hal/uc2_log.h>
#include <uc2_hal/uc2_ledc.h>
#include <nvs.h>
#include <math.h>

// NVS namespace (was uc2::Preferences hPreferences "heat").
#define HEAT_NVS_NS "heat"

// ---- Shared state (was namespace statics in the old header) ----
bool   heat_active          = false;
bool   heat_was_active      = false;
float  heat_pid_kp          = 10;
float  heat_pid_ki          = 0.1;
float  heat_pid_kd          = 0.1;
float  heat_pid_target      = 500;
float  heat_pid_updaterate  = 200000; // ms
int    heat_h_timeout       = 200000; // change in temperature in 60 seconds otherwise we stop
int   heat_max_pwm_value    = 511;

// ---- Internal state ----
static float  error_run_sum   = 0;
static float  previous_error  = 0;
static int    pwm_max_value   = 1023;
static int    pwm_min_value   = 0;
static long   t_temp_control_started = 0;
static float  temp_temp_control_started = 0.0f;

// timing variables
static unsigned long start_millis;
static unsigned long current_millis;

static int  n_analogin_avg = 50;

void heat_loop(void)
{
#if defined(LASER_CONTROLLER) && defined(DS18B20_CONTROLLER)
	int pwmPin = pinConfig.LASER_0;
	if (pwmPin < 0)
	{
		return;
	}

	// we need a protection against temperature runaway and overshoot
	// get modules
	int pwmChannel = laser_pwm_channel[0];

	if (heat_active && ((millis() - start_millis) >= heat_pid_updaterate))
	{

		// get rid of noise?
		float temperatureValueAvg = ds18b20_get_current_value_celcius();
		float pwmValue = 0;
		if (temperatureValueAvg < 60)
		{
			pwmValue = heat_return_control_value(heat_pid_target, temperatureValueAvg, heat_pid_kp, heat_pid_ki, heat_pid_kd);
			if (pwmValue > heat_max_pwm_value)
			{
				pwmValue = heat_max_pwm_value;
			}
		}

		// some safety mechanisms
		// if  temperature is not in a certain band after a time X, we have a runaway
		if (fabsf(heat_pid_target - temperatureValueAvg) > 1 and (millis() - t_temp_control_started) > heat_h_timeout and
			heat_h_timeout > 0)
		{
			heat_active = false;
			pwmValue = 0;
			log_e("HeatController::loop: temperature runaway detected, stopping heat");
			{
				nvs_handle_t h = 0;
				if (nvs_open(HEAT_NVS_NS, NVS_READWRITE, &h) == ESP_OK)
				{
					nvs_set_i32(h, "heatactive", heat_active ? 1 : 0);
					nvs_commit(h);
					nvs_close(h);
				}
			}
		}
		log_i("Heat: %f, pwmValue: %f, pwmChannel: %i", temperatureValueAvg, pwmValue, pwmChannel);
		laser_set_pwm((int)pwmValue, pwmChannel);
		start_millis = millis();
		heat_was_active = true;
	}
	else if (heat_active == false and heat_was_active == true)
	{ // if we are not using it, turn it off - but only once!
		if (ledcRead(pwmChannel) != 0)
		{
			log_i("Heat: %f, pwmValue: %f", ds18b20_get_current_value_celcius(), 0.0f);
			laser_set_pwm(0, pwmChannel);
		}
		heat_was_active = false;
	}

#endif
}

long heat_return_control_value(float controlTarget, float analoginValue, float Kp, float Ki, float Kd)
{
	float analoginOffset = 0.;
	float maxError = 1.;
	float error = (controlTarget - (analoginValue - analoginOffset)) / maxError;
	float cP = Kp * error;
	float cI = Ki * error_run_sum;
	float cD = Kd * (error - previous_error);
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

	error_run_sum = error_run_sum + error;
	previous_error = error;

	// 	if (isDEBUG)
	// log_d("analoginValue: " + String(analoginValue) + ", P: " + String(cP) + ", I: " + String(cI) + ", D: " + String(cD) + ", errorRunSum: " + String(errorRunSum) + ", previousError: " + String(previousError) + ", stepperOut: " + String(stepperOut));
	return stepperOut;
}

// Custom function accessible by the API
int heat_act(cJSON *ob)
{
	int qid = cjson_tool_get_int(ob, "qid", 0);
#if defined(LASER_CONTROLLER) && defined(DS18B20_CONTROLLER)
	// {"task": "/heat_act", "active":1, "Kp":1000, "Ki":0.1, "Kd":0.1, "target":22, "timeout":600000, "updaterate":1000, "qid":1}
	// {"task": "/heat_act", "active":0}
	nvs_handle_t h = 0;
	bool nvs_ok = (nvs_open(HEAT_NVS_NS, NVS_READWRITE, &h) == ESP_OK);

	// if json has key "active" then we set the heat active or not
	if (cJSON_HasObjectItem(ob, key_heatactive))
	{
		heat_active = cjson_tool_get_int(ob, key_heatactive, 0) != 0;
		if (nvs_ok) nvs_set_i32(h, "heatactive", heat_active ? 1 : 0);
	}
	else
	{
		heat_active = false;
	}
	// if json has key "Kp" then we set the heat active or not
	if (cJSON_HasObjectItem(ob, key_Kp))
	{
		heat_pid_kp = (float)cjson_tool_get_int(ob, key_Kp, 0);
		if (nvs_ok) nvs_set_f32(h, "temp_pid_Kp", heat_pid_kp);
	}
	else
	{
		heat_pid_kp = 10;
	}
	// if json has key "Ki" then we set the heat active or not
	if (cJSON_HasObjectItem(ob, key_Ki))
	{
		heat_pid_ki = (float)cjson_tool_get_int(ob, key_Ki, 0);
		if (nvs_ok) nvs_set_f32(h, "temp_pid_Ki", heat_pid_ki);
	}
	else
	{
		heat_pid_ki = 0.1;
	}
	// if json has key "Kd" then we set the heat active or not
	if (cJSON_HasObjectItem(ob, key_Kd))
	{
		heat_pid_kd = (float)cjson_tool_get_int(ob, key_Kd, 0);
		if (nvs_ok) nvs_set_f32(h, "temp_pid_Kd", heat_pid_kd);
	}
	else
	{
		heat_pid_kd = 0.1;
	}
	// if json has key "target" then we set the heat active or not
	if (cJSON_HasObjectItem(ob, key_target))
	{
		heat_pid_target = (float)cjson_tool_get_int(ob, key_target, 0);
		if (nvs_ok) nvs_set_f32(h, "temp_pid_target", heat_pid_target);
	}
	else
	{
		heat_active = false;
		if (nvs_ok) nvs_close(h);
		return 0;
	}
	// if json has key "updaterate" then we set the heat active or not
	if (cJSON_HasObjectItem(ob, key_heat_updaterate))
	{
		heat_pid_updaterate = (float)cjson_tool_get_int(ob, key_heat_updaterate, 0);
		if (nvs_ok) nvs_set_f32(h, "temp_pid_updaterate", heat_pid_updaterate);
	}
	else
	{
		heat_pid_updaterate = 1000;
	}

	// if json has key "hTimeout" then we set the heat active or not
	if (cJSON_HasObjectItem(ob, key_hTimeout))
	{
		heat_h_timeout = cjson_tool_get_int(ob, key_hTimeout, 0);
		if (nvs_ok) nvs_set_i32(h, "hTimeout", heat_h_timeout);
	}
	else
	{
		heat_h_timeout = -1;
		if (nvs_ok) nvs_set_i32(h, "hTimeout", heat_h_timeout);
	}

	// some safety mechanisms
	t_temp_control_started = millis();
	temp_temp_control_started = ds18b20_get_current_value_celcius();
	if (nvs_ok)
	{
		nvs_set_i64(h, "t_tempControlStarted", (int64_t)t_temp_control_started);
		nvs_set_f32(h, "temp_tempControlStarted", temp_temp_control_started);
		nvs_commit(h);
		nvs_close(h);
	}
	log_d("Heat_act_fct");
	log_i("Heat_active: %d, temp_pid_Kp: %f, temp_pid_Ki: %f, temp_pid_Kd: %f, temp_pid_target: %f, temp_pid_updaterate: %f", heat_active, heat_pid_kp, heat_pid_ki, heat_pid_kd, heat_pid_target, heat_pid_updaterate);
#endif
	return qid;
}

// Custom function accessible by the API
cJSON *heat_get(cJSON *ob)
{
	(void)ob;
	cJSON *ret = cJSON_CreateObject();
#if defined(LASER_CONTROLLER) && defined(DS18B20_CONTROLLER)
	// {"task": "/heat_get"}
	float temperatureValueAvg = 9999.0f;
	temperatureValueAvg = ds18b20_get_current_value_celcius();
	cjson_tool_set_float(ret, key_heat, temperatureValueAvg);
#endif
	return ret;
}

void heat_setup(void)
{
#if defined(LASER_CONTROLLER) && defined(DS18B20_CONTROLLER)
	log_d("Setup Heat");
	start_millis = millis();

	// load NVS (was hPreferences "heat")
	{
		nvs_handle_t h = 0;
		if (nvs_open(HEAT_NVS_NS, NVS_READONLY, &h) == ESP_OK)
		{
			int32_t iv = 0;
			float fv = 0;
			if (nvs_get_i32(h, "heatactive", &iv) == ESP_OK)
				heat_active = (iv != 0);
			if (nvs_get_f32(h, "temp_pid_Kp", &fv) == ESP_OK)
				heat_pid_kp = fv;
			else
				heat_pid_kp = 10;
			if (nvs_get_f32(h, "temp_pid_Ki", &fv) == ESP_OK)
				heat_pid_ki = fv;
			else
				heat_pid_ki = 0.1;
			if (nvs_get_f32(h, "temp_pid_Kd", &fv) == ESP_OK)
				heat_pid_kd = fv;
			else
				heat_pid_kd = 0.1;
			if (nvs_get_f32(h, "temp_pid_target", &fv) == ESP_OK)
				heat_pid_target = fv;
			else
				heat_pid_target = 37;
			if (nvs_get_f32(h, "temp_pid_updaterate", &fv) == ESP_OK)
				heat_pid_updaterate = fv;
			else
				heat_pid_updaterate = 1000;
			if (nvs_get_i32(h, "hTimeout", &iv) == ESP_OK)
				heat_h_timeout = (int)iv;
			else
				heat_h_timeout = -1;
			nvs_close(h);
		}
	}
	log_i("Heat_active: %d, temp_pid_Kp: %f, temp_pid_Ki: %f, temp_pid_Kd: %f, temp_pid_target: %f, temp_pid_updaterate: %f", heat_active, heat_pid_kp, heat_pid_ki, heat_pid_kd, heat_pid_target, heat_pid_updaterate);
	if (heat_active)
	{
		// some safety mechanisms
		t_temp_control_started = millis();
		temp_temp_control_started = ds18b20_get_current_value_celcius();
	}
#endif
}
