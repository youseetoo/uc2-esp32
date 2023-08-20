#include "State.h"
#include "esp_log.h"
#include "../config/ConfigController.h"
#include "Preferences.h"

State::State() : Module() { log_i("ctor"); }
State::~State() { log_i("~ctor"); }



void State::setup()
{
	log_d("Setup State");
}

void State::loop()
{
}

// {"task":"/state_act", "restart":1}
// {"task":"/state_act", "delay":1000}
// {"task":"/state_act", "isBusy":1}
// {"task":"/state_act", "resetPreferences":1}

// Custom function accessible by the API
int State::act(cJSON *  doc)
{
	// here you can do something
	log_i("state_act_fct");
		
	// get the command queue id from serial if available and add that to the return json
	int qid = getJsonInt(doc, keyQueueID);

	cJSON * restart = cJSON_GetObjectItemCaseSensitive(doc,"restart");
	// assign default values to thhe variables
	if (restart != NULL)
	{
		//{"task": "/state_act", "restart": 1}
		ESP.restart();
	}
	// assign default values to thhe variables
	cJSON * del = cJSON_GetObjectItemCaseSensitive(doc,"delay");
	if (del != NULL)
	{
		int mdelayms = del->valueint;
		delay(mdelayms);
	}
	cJSON * BUSY = cJSON_GetObjectItemCaseSensitive(doc,"isBusy");
	if (BUSY != NULL)
	{
		isBusy = BUSY->valueint;
	}
	cJSON * reset = cJSON_GetObjectItemCaseSensitive(doc,"resetPrefs");
	if (reset != NULL){
		log_i("resetPreferences");
		Preferences preferences;
		const char *prefNamespace = "UC2";
		preferences.begin(prefNamespace, false);
		preferences.clear();
		preferences.end();
		ESP.restart();
		return true;
	}
	return qid;
}

// Custom function accessible by the API
cJSON *  State::get(cJSON *  docin)
{	// {"task":"/state_get", "qid":2}
	// get the command queue id from serial if available and add that to the return json
	int qid = getJsonInt(docin, "qid");
	
	// create the return json document
	cJSON * doc = cJSON_CreateObject(); 

	// GET SOME PARAMETERS HERE
	cJSON * BUSY = cJSON_GetObjectItemCaseSensitive(doc,"isBusy");
	if (BUSY != NULL)
	{
		cJSON_AddItemToObject(doc,"isBusy", cJSON_CreateNumber(((int)isBusy)));
	}
	else
	{
		cJSON_AddItemToObject(doc,"identifier_name", cJSON_CreateString(identifier_name));
		cJSON_AddItemToObject(doc,"identifier_id", cJSON_CreateString(identifier_id));
		cJSON_AddItemToObject(doc,"identifier_date", cJSON_CreateString(identifier_date));
		cJSON_AddItemToObject(doc,"identifier_author", cJSON_CreateString(identifier_author));
		cJSON_AddItemToObject(doc,"IDENTIFIER_NAME", cJSON_CreateString(IDENTIFIER_NAME));
		cJSON_AddItemToObject(doc,"configIsSet", cJSON_CreateNumber(config_set));
		cJSON_AddItemToObject(doc,"pindef", cJSON_CreateString(pinConfig.pindefName));
		cJSON_AddItemToObject(doc,keyQueueID, cJSON_CreateNumber(qid));
	}
	return doc;
}

void State::printInfo()
{
	if (DEBUG)
		log_i("You can use this software by sending JSON strings, A full documentation can be found here:");
	if (DEBUG)
		log_i("https://github.com/openUC2/UC2-REST/");
	// log_i("A first try can be: \{\"task\": \"/state_get\"");
}


