#include "DigitalInController.h"

// This is for digitalinout

namespace RestApi
{
	void DigitalIn_act()
	{
		serialize(moduleController.get(AvailableModules::digitalin)->act(deserialize()));
	}

	void DigitalIn_get()
	{
		serialize(moduleController.get(AvailableModules::digitalin)->get(deserialize()));
	}
}

DigitalInController::DigitalInController(/* args */){};
DigitalInController::~DigitalInController(){};

// Custom function accessible by the API
int DigitalInController::act(DynamicJsonDocument jsonDocument)
{
	// here you can do something
	Serial.println("digitalin_act_fct");
	return 1;
}

// Custom function accessible by the API
DynamicJsonDocument DigitalInController::get(DynamicJsonDocument jsonDocument)
{

	// GET SOME PARAMETERS HERE
	int digitalinid = jsonDocument["digitalinid"];
	int digitalinpin = 0;
	int digitalinval = 0;

	// RESET THE JSON DOCUMENT
	jsonDocument.clear();

	// cretae new json document
	DynamicJsonDocument doc(1024);
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

	doc["digitalinid"] = digitalinid;
	doc["digitalinval"] = digitalinval;
	doc["digitalinpin"] = digitalinpin;
	return doc;
}

void DigitalInController::setup()
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

void DigitalInController::loop()
{

	//FIXME: Never reaches this position..

	// readout digital pins one by one
	digitalin_val_1 = digitalRead(pinConfig.DIGITAL_IN_1);
	digitalin_val_2 = digitalRead(pinConfig.DIGITAL_IN_2);
	digitalin_val_3 = digitalRead(pinConfig.DIGITAL_IN_3);
	//log_i("digitalin_val_1: %i, digitalin_val_2: %i, digitalin_val_3: %i", digitalin_val_1, digitalin_val_2, digitalin_val_3);
}

