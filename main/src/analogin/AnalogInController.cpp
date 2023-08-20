#include "AnalogInController.h"

AnalogInController::AnalogInController(/* args */){};
AnalogInController::~AnalogInController(){};

void AnalogInController::loop() {}

// Custom function accessible by the API
int AnalogInController::act(cJSON* ob)
{
	cJSON *monitor_json = ob;
	// here you can do something
	log_d("readanalogin_act_fct");
	int readanaloginID = cJSON_GetObjectItemCaseSensitive(monitor_json, key_N_analogin_avg)->valueint; //(int)(ob)[key_readanaloginID];
	int mN_analogin_avg = N_analogin_avg;
	if (readanaloginID != NULL)
		mN_analogin_avg = readanaloginID;
	int analoginpin = 0;
	switch (readanaloginID)
	{
	case 0:
		analoginpin = pinConfig.analogin_PIN_0;
		break;
	case 1:
		analoginpin = pinConfig.analogin_PIN_1;
		break;
	case 2:
		analoginpin = pinConfig.analogin_PIN_2;
		break;
	}

	float analoginValueAvg = 0;
	for (int imeas = 0; imeas < N_analogin_avg; imeas++)
	{
		analoginValueAvg += analogRead(analoginpin);
	}
	//float returnValue = (float)analoginValueAvg / (float)N_analogin_avg;

	return 1;
}

cJSON* AnalogInController::get(cJSON* ob)
{
	log_d("readanalogin_set_fct");
	
	cJSON *monitor_json = ob;
	int readanaloginID = cJSON_GetObjectItemCaseSensitive(monitor_json, key_readanaloginID)->valueint; //(int)(ob)[key_readanaloginID];
	int readanaloginPIN = cJSON_GetObjectItemCaseSensitive(monitor_json, key_readanaloginPIN)->valueint; //(int)(ob)[key_readanaloginPIN];
	N_analogin_avg = cJSON_GetObjectItemCaseSensitive(monitor_json, key_readanaloginPIN)->valueint; //(int)(ob)[key_N_analogin_avg];
	cJSON *monitor = cJSON_CreateObject();
    cJSON *analogholder = NULL;
    cJSON *x = NULL;
    cJSON *y = NULL;
    x = cJSON_CreateNumber(readanaloginPIN);
    y = cJSON_CreateNumber(readanaloginID);
    cJSON_AddItemToObject(monitor, key_readanaloginPIN, x);
    cJSON_AddItemToObject(monitor, key_readanaloginID, y);
	return monitor;
}

void AnalogInController::setup()
{
	log_d("Setup AnalogInController");
}
