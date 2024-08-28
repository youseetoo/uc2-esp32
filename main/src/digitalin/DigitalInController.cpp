#include <PinConfig.h>
#include "DigitalInController.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "cJsonTool.h"
#include "../i2c/tca_controller.h"

namespace DigitalInController
{
	// Custom function accessible by the API
	int act(cJSON *jsonDocument)
	{
		int qid = cJsonTool::getJsonInt(jsonDocument, "qid");
		// here you can do something
		log_d("digitalin_act_fct");
		return qid;
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
		digitalinval = getDigitalVal(digitalinid);
		monitor_json = cJSON_CreateObject();
		cJSON *digitalinholder = cJSON_CreateObject();
		cJSON_AddItemToObject(monitor_json, key_digitalin, digitalinholder);
		cJSON *id = cJSON_CreateNumber(digitalinid);
		cJSON_AddItemToObject(digitalinholder, "digitalinid", id);
		cJSON *val = cJSON_CreateNumber(digitalinval);
		cJSON_AddItemToObject(digitalinholder, "digitalinval", val);
		return monitor_json;
	}

	bool getDigitalVal(int digitalinid)
	{
		if (digitalinid == 1)
		{
			if (pinConfig.I2C_SCL > 0){
				return tca_controller::tca_read_endstop(pinConfig.DIGITAL_IN_1);
			}
			else{
				return digitalRead(pinConfig.DIGITAL_IN_1);
			}
		}
		else if (digitalinid == 2)
		{
			if (pinConfig.I2C_SCL > 0){
				return tca_controller::tca_read_endstop(pinConfig.DIGITAL_IN_2);
			}
			else{
				return digitalRead(pinConfig.DIGITAL_IN_2);
			}
		}
		else if (digitalinid == 3)
		{
			if (pinConfig.I2C_SCL > 0){
				return tca_controller::tca_read_endstop(pinConfig.DIGITAL_IN_3);
			}
			else{
				return digitalRead(pinConfig.DIGITAL_IN_3);
			}
		}
		return false;
	}
		
	void setup()
	{
		if (pinConfig.I2C_SCL < 0)
		{
			log_i("Setting Up digitalin");
			/* setup the output nodes and reset them to 0*/
			log_i("DigitalIn 1: %i", pinConfig.DIGITAL_IN_1);
			log_i("DigitalIn 2: %i", pinConfig.DIGITAL_IN_2);
			log_i("DigitalIn 3: %i", pinConfig.DIGITAL_IN_3);
			pinMode(pinConfig.DIGITAL_IN_1, INPUT_PULLDOWN);
			pinMode(pinConfig.DIGITAL_IN_2, INPUT_PULLDOWN);
			pinMode(pinConfig.DIGITAL_IN_3, INPUT_PULLDOWN);
		}
		else
		{
			log_i("Setting Up TCA9535 for digitalin");
		}
	}

	void loop()
	{

		// FIXME: Never reaches this position..
		digitalin_val_1 = getDigitalVal(1);
		digitalin_val_2 = getDigitalVal(2);
		digitalin_val_3 = getDigitalVal(3);
		//log_i("digitalin_val_1: %i, digitalin_val_2: %i, digitalin_val_3: %i", digitalin_val_1, digitalin_val_2, digitalin_val_3);
	}
} // namespace DigitalInController
