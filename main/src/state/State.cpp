#include <PinConfig.h>
#include "State.h"
#include "esp_log.h"
#include "../config/ConfigController.h"
#include "Preferences.h"
#include "cJsonTool.h"
#include "Arduino.h"
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
	// {"task":"/state_act", "debug":0} // 0-5

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
		// set debug level
		cJSON *debug = cJSON_GetObjectItemCaseSensitive(doc, "debug");
		if (debug != NULL)
		{
			DEBUG = debug->valueint;
			esp_log_level_set("MessageController", (esp_log_level_t)DEBUG);
		}
		// assign default values to thhe variables
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

	cJSON *get(cJSON *docin)
	{
		// Custom function accessible by the API
		// return json {"state":{...}} as qid
		// {"task":"/state_get",  "qid":1}
		// {"task":"/state_get", "isBusy":1}
		// {"task":"/state_get", "heap":1}
		// This returns: {"identifier_name":UC2_Feather, "identifier_id":V2.0, "identifier_date":__DATE__ __TIME__, "identifier_author":BD, "IDENTIFIER_NAME":uc2-esp, "configIsSet":0, "pindef":UC2}
		cJSON *doc = cJSON_CreateObject();
		cJSON *st = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, "state", st);

		// GET SOME PARAMETERS HERE 
		int qid = cJsonTool::getJsonInt(docin, "qid");
		cJSON *BUSY = cJSON_GetObjectItemCaseSensitive(docin, "isBusy");
		cJSON *HEAP = cJSON_GetObjectItemCaseSensitive(docin, "heap");
		if (BUSY != NULL)
		{
			cJSON_AddItemToObject(st, "isBusy", cJSON_CreateNumber(((int)isBusy)));
		}
		else if (HEAP != NULL)
		{
			cJSON_AddItemToObject(st, "heap", cJSON_CreateNumber(ESP.getFreeHeap()));
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
			//cJSON_AddItemToObject(st, "heap", cJSON_CreateNumber(ESP.getFreeHeap()));
		}
		cJSON_AddItemToObject(doc, "qid", cJSON_CreateNumber(qid));

		return doc;
	}

	void setBusy(bool busy)
	{
		isBusy = busy;
	}

	bool getBusy()
	{
		return isBusy;
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
#ifdef LED_CONTROLLER
		cJSON_AddItemToObject(mod, keyLed, cJSON_CreateNumber(pinConfig.LED_PIN >= 0));
#endif
#ifdef MOTOR_CONTROLLER
		cJSON_AddItemToObject(mod, key_motor, cJSON_CreateNumber(pinConfig.MOTOR_ENABLE >= 0));
#endif
#ifdef GPIO_CONTROLLER
		cJSON_AddItemToObject(mod, key_gpio, cJSON_CreateNumber((pinConfig.GPIO_PIN_1 >= 0 || pinConfig.GPIO_PIN_2 >= 0 || pinConfig.GPIO_PIN_3 >= 0)));
#endif
#ifdef ENCODER_CONTROLLER
		cJSON_AddItemToObject(mod, key_encoder, cJSON_CreateNumber((pinConfig.X_CAL_CLK >= 0 || pinConfig.Y_CAL_CLK >= 0 || pinConfig.Z_CAL_CLK >= 0)));
#endif
#ifdef MESSAGE_CONTROLLER
cJSON_AddItemToObject(mod, key_message, cJSON_CreateNumber(1));
#endif
#ifdef HOME_MOTOR
		cJSON_AddItemToObject(mod, key_home, cJSON_CreateNumber((pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)));
#endif
		cJSON_AddItemToObject(mod, key_analogin, cJSON_CreateNumber((pinConfig.analogin_PIN_0 >= 0 || pinConfig.analogin_PIN_1 >= 0 || pinConfig.analogin_PIN_2 >= 0 || pinConfig.analogin_PIN_3 >= 0)));
		cJSON_AddItemToObject(mod, key_pid, cJSON_CreateNumber((pinConfig.pid1 >= 0 || pinConfig.pid2 >= 0 || pinConfig.pid3 >= 0)));
		cJSON_AddItemToObject(mod, key_laser, cJSON_CreateNumber((pinConfig.LASER_1 >= 0 || pinConfig.LASER_2 >= 0 || pinConfig.LASER_3 >= 0)));
		cJSON_AddItemToObject(mod, key_dac, cJSON_CreateNumber((pinConfig.dac_fake_1 >= 0 || pinConfig.dac_fake_2 >= 0)));
		cJSON_AddItemToObject(mod, key_analogout, cJSON_CreateNumber((pinConfig.analogout_PIN_1 >= 0 || pinConfig.analogout_PIN_2 >= 0 || pinConfig.analogout_PIN_3 >= 0)));
		cJSON_AddItemToObject(mod, key_digitalout, cJSON_CreateNumber((pinConfig.DIGITAL_OUT_1 >= 0 || pinConfig.DIGITAL_OUT_2 >= 0 || pinConfig.DIGITAL_OUT_3 >= 0)));
		cJSON_AddItemToObject(mod, key_digitalin, cJSON_CreateNumber((pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)));
#ifdef SCANNER_CONTROLLER
		cJSON_AddItemToObject(mod, key_scanner, cJSON_CreateNumber(1));
#endif

		return doc;
	}
}
