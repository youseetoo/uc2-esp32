#include "DigitalOutController.h"
#include "Arduino.h"
#include "PinConfig.h"
#include "JsonKeys.h"
#include "cJsonTool.h"

namespace DigitalOutController
{
	// Custom function accessible by the API
	int act(cJSON *jsonDocument)
	{
		// here you can do something
		log_d("digitalout_act_fct");
		isBusy = true;
		int triggerdelay = 10;

		// set default parameters
		// is trigger active?
		is_digital_trigger_1 = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut1IsTrigger);
		is_digital_trigger_2 = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut2IsTrigger);
		is_digital_trigger_3 = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut3IsTrigger);

		// on-delay
		digitalout_trigger_delay_1_on = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut1TriggerDelayOn);
		digitalout_trigger_delay_2_on = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut2TriggerDelayOn);
		digitalout_trigger_delay_3_on = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut3TriggerDelayOn);

		// off-delay
		digitalout_trigger_delay_1_off = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut1TriggerDelayOff);
		digitalout_trigger_delay_2_off = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut2TriggerDelayOff);
		digitalout_trigger_delay_3_off = cJsonTool::getJsonInt(jsonDocument, keyDigitalOut3TriggerDelayOff);
		int reset = cJsonTool::getJsonInt(jsonDocument, keyDigitalOutIsTriggerReset);

		// reset trigger
		if (reset == 1)
		{
			is_digital_trigger_1 = false;
			is_digital_trigger_2 = false;
			is_digital_trigger_3 = false;
		}

		//
		int digitaloutid = cJsonTool::getJsonInt(jsonDocument, keyDigitalOutid);
		int digitaloutval = cJsonTool::getJsonInt(jsonDocument, keyDigitalOutVal);

		// print debugging information
		log_d("digitaloutid %d", digitaloutid);
		log_d("digitaloutval %d", digitaloutval);
		log_d("digitalout_trigger_delay_1_on %d", digitalout_trigger_delay_1_on);
		log_d("digitalout_trigger_delay_1_off %d", digitalout_trigger_delay_1_off);
		log_d("digitalout_trigger_delay_2_on %d", digitalout_trigger_delay_2_on);
		log_d("digitalout_trigger_delay_2_off %d", digitalout_trigger_delay_2_off);
		log_d("digitalout_trigger_delay_3_on %d", digitalout_trigger_delay_3_on);
		log_d("digitalout_trigger_delay_3_off %d", digitalout_trigger_delay_3_off);
		log_d("is_digital_trigger_1 %d", is_digital_trigger_1);
		log_d("is_digital_trigger_2 %d", is_digital_trigger_2);
		log_d("is_digital_trigger_3 %d", is_digital_trigger_3);

		if (digitaloutid == 1)
		{
			digitalout_val_1 = digitaloutval;
			if (digitaloutval == -1)
			{
				// perform trigger
				digitalWrite(pinConfig.DIGITAL_OUT_1, HIGH);
				delay(triggerdelay);
				digitalWrite(pinConfig.DIGITAL_OUT_1, LOW);
			}
			else
			{
				digitalWrite(pinConfig.DIGITAL_OUT_1, digitalout_val_1);
				log_d("DIGITAL_OUT ");
				log_d(pinConfig.DIGITAL_OUT_1);
			}
		}
		else if (digitaloutid == 2)
		{
			digitalout_val_2 = digitaloutval;
			if (digitaloutval == -1)
			{
				// perform trigger
				digitalWrite(pinConfig.DIGITAL_OUT_2, HIGH);
				delay(triggerdelay);
				digitalWrite(pinConfig.DIGITAL_OUT_2, LOW);
			}
			else
			{
				digitalWrite(pinConfig.DIGITAL_OUT_2, digitalout_val_2);
				log_d("DIGITAL_OUT ");
				log_d(pinConfig.DIGITAL_OUT_2);
			}
		}
		else if (digitaloutid == 3)
		{
			digitalout_val_3 = digitaloutval;
			if (digitaloutval == -1)
			{
				// perform trigger
				digitalWrite(pinConfig.DIGITAL_OUT_3, HIGH);
				delay(triggerdelay);
				digitalWrite(pinConfig.DIGITAL_OUT_3, LOW);
			}
			else
			{
				digitalWrite(pinConfig.DIGITAL_OUT_3, digitalout_val_3);
				log_d("DIGITAL_OUT ");
				log_d(pinConfig.DIGITAL_OUT_3);
			}
		}
		return 1;
	}

	// Custom function accessible by the API
	cJSON *get(cJSON *jsonDocument)
	{
		// GET SOME PARAMETERS HERE
		int digitaloutid = cJsonTool::getJsonInt(jsonDocument, "digitaloutid");

		int digitaloutpin = 0;
		int digitaloutval = 0;

		if (digitaloutid == 1)
		{
			if (DEBUG)
				log_d("digitalout 1");
			digitaloutpin = pinConfig.DIGITAL_OUT_1;
			digitaloutval = digitalout_val_1;
		}
		else if (digitaloutid == 2)
		{
			if (DEBUG)
				log_d("AXIS 2");
			if (DEBUG)
				log_d("digitalout 2");
			digitaloutpin = pinConfig.DIGITAL_OUT_2;
			digitaloutval = digitalout_val_2;
		}
		else if (digitaloutid == 3)
		{
			if (DEBUG)
				log_d("AXIS 3");
			if (DEBUG)
				log_d("digitalout 1");
			digitaloutpin = pinConfig.DIGITAL_OUT_3;
			digitaloutval = digitalout_val_3;
		}

		jsonDocument = cJSON_CreateObject();
		cJSON *douth = cJSON_CreateObject();
		cJSON_AddItemToObject(jsonDocument, key_digitalout, douth);
		cJsonTool::setJsonInt(douth, "digitaloutid", digitaloutid);
		cJsonTool::setJsonInt(douth, "digitaloutval", digitaloutval);
		cJsonTool::setJsonInt(douth, "digitaloutpin", digitaloutpin);
		return jsonDocument;
	}

	void setup()
	{
		log_d("Setting Up digitalout");
		/* setup the output nodes and reset them to 0*/
		pinMode(pinConfig.DIGITAL_OUT_1, OUTPUT);

		digitalWrite(pinConfig.DIGITAL_OUT_1, HIGH);
		delay(50);
		digitalWrite(pinConfig.DIGITAL_OUT_1, LOW);

		pinMode(pinConfig.DIGITAL_OUT_2, OUTPUT);
		digitalWrite(pinConfig.DIGITAL_OUT_2, HIGH);
		delay(50);
		digitalWrite(pinConfig.DIGITAL_OUT_2, LOW);

		pinMode(pinConfig.DIGITAL_OUT_3, OUTPUT);
		digitalWrite(pinConfig.DIGITAL_OUT_3, HIGH);
		delay(50);
		digitalWrite(pinConfig.DIGITAL_OUT_3, LOW);
	}

	void loop()
	{

		// run trigger table based on previously set parameters
		if (is_digital_trigger_1)
		{
			digitalWrite(pinConfig.DIGITAL_OUT_1, HIGH);
			// log_i("DIGITAL_OUT_1 HIGH");
			delay(digitalout_trigger_delay_1_on);
			digitalWrite(pinConfig.DIGITAL_OUT_1, LOW);
			// log_i("DIGITAL_OUT_1 LOW");
			delay(digitalout_trigger_delay_1_off);
		}
		if (is_digital_trigger_2)
		{
			digitalWrite(pinConfig.DIGITAL_OUT_2, HIGH);
			// log_i("DIGITAL_OUT_2 HIGH");
			delay(digitalout_trigger_delay_2_on);
			digitalWrite(pinConfig.DIGITAL_OUT_2, LOW);
			// log_i("DIGITAL_OUT_2 LOW");
			delay(digitalout_trigger_delay_2_off);
		}
		if (is_digital_trigger_3)
		{
			digitalWrite(pinConfig.DIGITAL_OUT_3, HIGH);
			// log_i("DIGITAL_OUT_3 HIGH");
			delay(digitalout_trigger_delay_3_on);
			digitalWrite(pinConfig.DIGITAL_OUT_3, LOW);
			// log_i("DIGITAL_OUT_3 LOW");
			delay(digitalout_trigger_delay_3_off);
		}
	}
}