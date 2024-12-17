#include <PinConfig.h>
#include "AnalogOutController.h"

#include "JsonKeys.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/periph_ctrl.h"
#include "soc/ledc_reg.h"
namespace AnalogOutController
{

	void setup()
	{
		log_d("Setup AnalogOutController");
		/* setup the PWM ports and reset them to 0*/
		if (pinConfig.analogout_PIN_1 > 0)
		{
			ledcSetup(PWM_CHANNEL_analogout_1, pwm_frequency, pwm_resolution);
			ledcAttachPin(pinConfig.analogout_PIN_1, PWM_CHANNEL_analogout_1);
			ledcWrite(PWM_CHANNEL_analogout_1, 0);
		}
		if (pinConfig.analogout_PIN_2 > 0)
		{
			ledcSetup(PWM_CHANNEL_analogout_2, pwm_frequency, pwm_resolution);
			ledcAttachPin(pinConfig.analogout_PIN_2, PWM_CHANNEL_analogout_2);
			ledcWrite(PWM_CHANNEL_analogout_2, 0);
		}
		if (pinConfig.analogout_PIN_3 > 0)
		{
			ledcSetup(PWM_CHANNEL_analogout_3, pwm_frequency, pwm_resolution);
			ledcAttachPin(pinConfig.analogout_PIN_3, PWM_CHANNEL_analogout_3);
			ledcWrite(PWM_CHANNEL_analogout_3, 0);
		}
	}

	void btcontroller_event(int left, int right, bool r1, bool r2, bool l1, bool l2)
	{
		if (left && pinConfig.analogout_PIN_1 >= 0)
		{
			// fine lens -
			analogout_val_1 -= 1;
			delay(50);
			ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
		}
		if (right and pinConfig.analogout_PIN_1 >= 0)
		{
			// fine lens +
			analogout_val_1 += 1;
			delay(50);
			ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
		}
		// unknown button
		/*if (PS4.data.button.start) {
		  // reset
		  analogout_val_1 = 0;
		  ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
		}*/
		if (r2 && pinConfig.analogout_PIN_1 >= 0)
		{
			// analogout_val_1++ coarse
			if ((analogout_val_1 + 1000 < pwm_max))
			{
				analogout_val_1 += 1000;
				ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
			}
		}
		if (l2 && pinConfig.analogout_PIN_1 >= 0)
		{
			// analogout_val_1-- coarse
			if ((analogout_val_1 - 1000 > 0))
			{
				analogout_val_1 -= 1000;
				ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
			}
		}

		if (l1 && pinConfig.analogout_PIN_1 >= 0)
		{
			// analogout_val_1 + semi coarse
			if ((analogout_val_1 + 100 < pwm_max))
			{
				analogout_val_1 += 100;
				ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
				// delay(100);
			}
		}
		if (r1 && pinConfig.analogout_PIN_1 >= 0)
		{
			// analogout_val_1 - semi coarse
			if ((analogout_val_1 - 100 > 0))
			{
				analogout_val_1 -= 100;
				ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
				// delay(50);
			}
		}
	}

	// Custom function accessible by the API
	int act(cJSON *ob)
	{
		// here you can do something
		log_d("analogout_act_fct");
		cJSON *monitor_json = ob;
		int qid = getJsonInt(ob, "qid");

		int analogoutid = cJSON_GetObjectItemCaseSensitive(monitor_json, "analogoutid")->valueint;	 //(ob)["analogoutid"];
		int analogoutval = cJSON_GetObjectItemCaseSensitive(monitor_json, "analogoutval")->valueint; //(ob)["analogoutval"];

		if (DEBUG)
		{
			log_d("analogoutid %i", analogoutid);
			log_d("analogoutval%i", analogoutval);
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
		return qid;
	}

	// Custom function accessible by the API
	cJSON *get(cJSON *jsonDocument)
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
		cJSON_AddItemToObject(monitor, key_analogout, analogholder);
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
}
