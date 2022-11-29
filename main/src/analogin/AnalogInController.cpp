#include "../../config.h"

#include "AnalogInController.h"

AnalogInController::AnalogInController(/* args */){};
AnalogInController::~AnalogInController(){};

void AnalogInController::loop() {}

// Custom function accessible by the API
int AnalogInController::act(DynamicJsonDocument ob)
{

	// here you can do something
	if (DEBUG)
		Serial.println("readanalogin_act_fct");
	int readanaloginID = (int)(ob)[key_readanaloginID];
	int mN_analogin_avg = N_analogin_avg;
	if (ob.containsKey(key_N_analogin_avg))
		mN_analogin_avg = (int)(ob)[key_N_analogin_avg];
	int analoginpin = 0;

	if (DEBUG)
		Serial.print(key_readanaloginID);
	Serial.println(readanaloginID);
	switch (readanaloginID)
	{
	case 0:
		analoginpin = pins.analogin_PIN_0;
		break;
	case 1:
		analoginpin = pins.analogin_PIN_1;
		break;
	case 2:
		analoginpin = pins.analogin_PIN_2;
		break;
	}

	float analoginValueAvg = 0;
	for (int imeas = 0; imeas < N_analogin_avg; imeas++)
	{
		analoginValueAvg += analogRead(analoginpin);
	}
	float returnValue = (float)analoginValueAvg / (float)N_analogin_avg;

	return 1;
}

DynamicJsonDocument AnalogInController::get(DynamicJsonDocument ob)
{
	if (DEBUG)
		Serial.println("readanalogin_set_fct");
	int readanaloginID = (int)(ob)[key_readanaloginID];
	int readanaloginPIN = (int)(ob)[key_readanaloginPIN];
	if (ob.containsKey(key_N_analogin_avg))
		N_analogin_avg = (int)(ob)[key_N_analogin_avg];

	switch (readanaloginID)
	{
	case 0:
		pins.analogin_PIN_0 = readanaloginPIN;
		break;
	case 1:
		pins.analogin_PIN_1 = readanaloginPIN;
		break;
	case 2:
		pins.analogin_PIN_2 = readanaloginPIN;
		break;
	}

	ob.clear();
	ob[key_readanaloginPIN] = readanaloginPIN;
	ob[key_readanaloginID] = readanaloginID;
	return ob;
}

// Custom function accessible by the API
int AnalogInController::set(DynamicJsonDocument jsonDocument)
{
	if (DEBUG)
		Serial.println("readanalogin_get_fct");
	int readanaloginID = (int)(jsonDocument)[key_readanaloginID];
	int readanaloginPIN = 0;
	switch (readanaloginID)
	{
	case 0:
		readanaloginPIN = pins.analogin_PIN_0;
		break;
	case 1:
		readanaloginPIN = pins.analogin_PIN_1;
		break;
	case 2:
		readanaloginPIN = pins.analogin_PIN_2;
		break;
	}
	return 1;
}

void AnalogInController::setup()
{
	if (DEBUG)
		Serial.println("Setting up analogins...");
}
