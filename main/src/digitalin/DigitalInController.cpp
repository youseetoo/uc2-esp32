#include "DigitalInController.h"
#include "Arduino.h"
#include "../../PinConfig.h"
#include "../../JsonKeys.h"

namespace DigitalInController
{
	// Custom function accessible by the API
	int act(cJSON *jsonDocument)
	{
		// here you can do something
		log_d("digitalin_act_fct");
		return 1;
	}

	// Custom function accessible by the API
	cJSON *get(cJSON *jsonDocument)
	{

		// GET SOME PARAMETERS HERE
		cJSON *monitor_json = jsonDocument;
		int digitalinid = cJSON_GetObjectItemCaseSensitive(monitor_json, "digitalinid")->valueint; // jsonDocument["digitalinid"];
		int digitalinpin = 0;
		int digitalinval = 0;

		// cretae new json document
		//
		if (digitalinid == 1)
		{
			digitalinpin = pinConfig.DIGITAL_IN_1;
			digitalinval = digitalRead(pinConfig.DIGITAL_IN_1);
		}
		else if (digitalinid == 2)
		{
			digitalinpin = pinConfig.DIGITAL_IN_2;
			digitalinval = digitalRead(pinConfig.DIGITAL_IN_2);
		}
		else if (digitalinid == 3)
		{
			digitalinpin = pinConfig.DIGITAL_IN_3;
			digitalinval = digitalRead(pinConfig.DIGITAL_IN_3);
		}
		monitor_json = cJSON_CreateObject();
		cJSON *digitalinholder = cJSON_CreateObject();
		cJSON_AddItemToObject(monitor_json, key_digitalin, digitalinholder);
		cJSON *id = cJSON_CreateNumber(digitalinid);
		cJSON_AddItemToObject(digitalinholder, "digitalinid", id);
		cJSON *val = cJSON_CreateNumber(digitalinval);
		cJSON_AddItemToObject(digitalinholder, "digitalinval", val);
		cJSON *pin = cJSON_CreateNumber(digitalinpin);
		cJSON_AddItemToObject(digitalinholder, "digitalinpin", pin);
		return monitor_json;
	}

	void setup()
	{
		log_i("Setting Up digitalin");
		/* setup the output nodes and reset them to 0*/
		log_i("DigitalIn 1: %i", pinConfig.DIGITAL_IN_1);
		pinMode(pinConfig.DIGITAL_IN_1, INPUT_PULLDOWN);
		log_i("DigitalIn 2: %i", pinConfig.DIGITAL_IN_2);
		pinMode(pinConfig.DIGITAL_IN_2, INPUT_PULLDOWN);
		log_i("DigitalIn 3: %i", pinConfig.DIGITAL_IN_3);
		pinMode(pinConfig.DIGITAL_IN_3, INPUT_PULLDOWN);
	}

	void loop()
	{

		// FIXME: Never reaches this position..

		// readout digital pins one by one
		digitalin_val_1 = digitalRead(pinConfig.DIGITAL_IN_1);
		digitalin_val_2 = digitalRead(pinConfig.DIGITAL_IN_2);
		digitalin_val_3 = digitalRead(pinConfig.DIGITAL_IN_3);
		// log_i("digitalin_val_1: %i, digitalin_val_2: %i, digitalin_val_3: %i", digitalin_val_1, digitalin_val_2, digitalin_val_3);
	}
}
