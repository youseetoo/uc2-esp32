#include "AnalogOutController.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/periph_ctrl.h"
#include "soc/ledc_reg.h"

AnalogOutController::AnalogOutController(){};
AnalogOutController::~AnalogOutController(){};

void AnalogOutController::loop() {}

void AnalogOutController::setup()
{
	log_d("Setup AnalogOutController");
	/* setup the PWM ports and reset them to 0*/
	ledcSetup(PWM_CHANNEL_analogout_1, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.analogout_PIN_1, PWM_CHANNEL_analogout_1);
	ledcWrite(PWM_CHANNEL_analogout_1, 0);

	ledcSetup(PWM_CHANNEL_analogout_2, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.analogout_PIN_2, PWM_CHANNEL_analogout_2);
	ledcWrite(PWM_CHANNEL_analogout_2, 0);
}

// Custom function accessible by the API
int AnalogOutController::act(cJSON* ob)
{
	// here you can do something
	log_d("analogout_act_fct");
	cJSON *monitor_json = ob;

	int analogoutid = cJSON_GetObjectItemCaseSensitive(monitor_json, "analogoutid")->valueint; //(ob)["analogoutid"];
	int analogoutval = cJSON_GetObjectItemCaseSensitive(monitor_json, "analogoutval")->valueint; //(ob)["analogoutval"];

	if (DEBUG)
	{
		log_d("analogoutid %i", analogoutid);
		log_d("analogoutval%i",  analogoutval);
	}

	if (analogoutid == 1)
	{
		analogout_val_1 = analogoutval;
		ledcWrite(PWM_CHANNEL_analogout_1, analogoutval);
	}
	else if (analogoutid == 2)
	{
		analogout_val_2 = analogoutval;
		ledcWrite(PWM_CHANNEL_analogout_2, analogoutval);
	}
	else if (analogoutid == 3)
	{
		analogout_val_3 = analogoutval;
		ledcWrite(PWM_CHANNEL_analogout_3, analogoutval);
	}
	return 1;
}

// Custom function accessible by the API
cJSON* AnalogOutController::get(cJSON* jsonDocument)
{

	cJSON *monitor_json = jsonDocument;
	// GET SOME PARAMETERS HERE
	int analogoutid = cJSON_GetObjectItemCaseSensitive(monitor_json, "analogoutid")->valueint; //(ob)["analogoutid"];
	int analogoutpin = 0;
	int analogoutval = 0;

	if (analogoutid == 1)
	{
		if (DEBUG)
			Serial.println("analogout 1");
		analogoutpin = pinConfig.analogout_PIN_1;
		analogoutval = analogout_val_1;
	}
	else if (analogoutid == 2)
	{
		if (DEBUG)
			Serial.println("AXIS 2");
		if (DEBUG)
			Serial.println("analogout 2");
		analogoutpin = pinConfig.analogout_PIN_2;
		analogoutval = analogout_val_2;
	}
	else if (analogoutid == 3)
	{
		if (DEBUG)
			Serial.println("AXIS 3");
		if (DEBUG)
			Serial.println("analogout 1");
		analogoutpin = pinConfig.analogout_PIN_3;
		analogoutval = analogout_val_3;
	}

	cJSON *monitor = cJSON_CreateObject();
    cJSON *analogholder = cJSON_CreateObject();
	cJSON_AddItemToObject(monitor,key_analogout, analogholder);
    cJSON *pin = NULL;
    cJSON *id = NULL;
    cJSON *val = NULL;
    pin = cJSON_CreateNumber(analogoutpin);
    id = cJSON_CreateNumber(analogoutid);
    val = cJSON_CreateNumber(analogoutval);
    cJSON_AddItemToObject(analogholder, "analogoutpin", pin);
    cJSON_AddItemToObject(analogholder, "analogoutval", val);
    cJSON_AddItemToObject(analogholder, "analogoutid", id);

	return monitor;
}
