#include "State.h"
#include "esp_log.h"
#include "../config/ConfigController.h"
#include "Preferences.h"
#include "cJsonTool.h"
#include "Arduino.h"
#include <PinConfig.h>
#include "JsonKeys.h"

namespace State
{

	void setup()
	{
		log_d("Setup State");
	}

	// {"task":"/state_act", "restart":1}
	// {"task":"/state_act", "delay":1000}
	// {"task":"/state_act", "isBusy":1}
	// {"task":"/state_act", "resetPreferences":1}

	// Custom function accessible by the API
	int act(cJSON *doc)
	{
		// here you can do something
		if (DEBUG)
			log_i("state_act_fct");

		cJSON *restart = cJSON_GetObjectItemCaseSensitive(doc, "restart");
		// assign default values to thhe variables
		if (restart != NULL)
		{
			//{"task": "/state_act", "restart": 1}
			ESP.restart();
		}
		// assign default values to thhe variables
		cJSON *del = cJSON_GetObjectItemCaseSensitive(doc, "delay");
		if (del != NULL)
		{
			int mdelayms = del->valueint;
			delay(mdelayms);
		}
		cJSON *BUSY = cJSON_GetObjectItemCaseSensitive(doc, "isBusy");
		if (BUSY != NULL)
		{
			isBusy = BUSY->valueint;
		}
		cJSON *reset = cJSON_GetObjectItemCaseSensitive(doc, "resetPrefs");
		if (reset != NULL)
		{
			log_i("resetPreferences");
			Preferences preferences;
			const char *prefNamespace = "UC2";
			preferences.begin(prefNamespace, false);
			preferences.clear();
			preferences.end();
			ESP.restart();
			return true;
		}
		return 1;
	}

	// Custom function accessible by the API
	// return json {"state":{...}} as qid
	cJSON *get(cJSON *docin)
	{
		cJSON *doc = cJSON_CreateObject();
		cJSON *st = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, "state", st);

		// GET SOME PARAMETERS HERE
		cJSON *BUSY = cJSON_GetObjectItemCaseSensitive(doc, "isBusy");
		if (BUSY != NULL)
		{
			cJSON_AddItemToObject(st, "isBusy", cJSON_CreateNumber(((int)isBusy)));
		}
		else
		{
			cJSON_AddItemToObject(st, "identifier_name", cJSON_CreateString(identifier_name));
			cJSON_AddItemToObject(st, "identifier_id", cJSON_CreateString(identifier_id));
			cJSON_AddItemToObject(st, "identifier_date", cJSON_CreateString(identifier_date));
			cJSON_AddItemToObject(st, "identifier_author", cJSON_CreateString(identifier_author));
			cJSON_AddItemToObject(st, "IDENTIFIER_NAME", cJSON_CreateString(IDENTIFIER_NAME));
			cJSON_AddItemToObject(st, "configIsSet", cJSON_CreateNumber(config_set));
			cJSON_AddItemToObject(st, "pindef", cJSON_CreateString(pinConfig.pindefName));
		}
		return doc;
	}

	void printInfo()
	{
		if (DEBUG)
			log_i("You can use this software by sending JSON strings, A full documentation can be found here:");
		if (DEBUG)
			log_i("https://github.com/openUC2/UC2-REST/");
		// log_i("A first try can be: \{\"task\": \"/state_get\"");
	}

	cJSON *getModules()
	{
		cJSON *doc = cJSON_CreateObject();
		cJSON *mod = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, key_modules, mod);
		cJSON_AddItemToObject(mod, keyLed, cJSON_CreateNumber(pinConfig.LED_PIN >= 0));
		cJSON_AddItemToObject(mod, key_motor, cJSON_CreateNumber(pinConfig.MOTOR_ENABLE >= 0));
		cJSON_AddItemToObject(mod, key_encoder, cJSON_CreateNumber((pinConfig.X_CAL_CLK >= 0 || pinConfig.Y_CAL_CLK >= 0 || pinConfig.Z_CAL_CLK >= 0)));
		cJSON_AddItemToObject(mod, key_home, cJSON_CreateNumber((pinConfig.PIN_DEF_END_X >= 0 || pinConfig.PIN_DEF_END_Y >= 0 || pinConfig.PIN_DEF_END_Z >= 0)));
		cJSON_AddItemToObject(mod, key_analogin, cJSON_CreateNumber((pinConfig.analogin_PIN_0 >= 0 || pinConfig.analogin_PIN_1 >= 0 || pinConfig.analogin_PIN_2 >= 0 || pinConfig.analogin_PIN_3 >= 0)));
		cJSON_AddItemToObject(mod, key_pid, cJSON_CreateNumber((pinConfig.pid1 >= 0 || pinConfig.pid2 >= 0 || pinConfig.pid3 >= 0)));
		cJSON_AddItemToObject(mod, key_laser, cJSON_CreateNumber((pinConfig.LASER_1 >= 0 || pinConfig.LASER_2 >= 0 || pinConfig.LASER_3 >= 0)));
		cJSON_AddItemToObject(mod, key_dac, cJSON_CreateNumber((pinConfig.dac_fake_1 >= 0 || pinConfig.dac_fake_2 >= 0)));
		cJSON_AddItemToObject(mod, key_analogout, cJSON_CreateNumber((pinConfig.analogout_PIN_1 >= 0 || pinConfig.analogout_PIN_2 >= 0 || pinConfig.analogout_PIN_3 >= 0)));
		cJSON_AddItemToObject(mod, key_digitalout, cJSON_CreateNumber((pinConfig.DIGITAL_OUT_1 >= 0 || pinConfig.DIGITAL_OUT_2 >= 0 || pinConfig.DIGITAL_OUT_3 >= 0)));
		cJSON_AddItemToObject(mod, key_digitalin, cJSON_CreateNumber((pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)));
		cJSON_AddItemToObject(mod, key_scanner, cJSON_CreateNumber(pinConfig.enableScanner));

		return doc;
	}
}
