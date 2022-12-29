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
DynamicJsonDocument DigitalInController::get(DynamicJsonDocument  jsonDocument)
{
	
	// GET SOME PARAMETERS HERE
	int digitalinid = jsonDocument["digitalinid"];
	int digitalinpin = 0;
	int digitalinval = 0;

	if (digitalinid == 1)
	{
		if (DEBUG)
			Serial.println("digitalin 1");
		digitalinpin = pinConfig.DIGITAL_IN_1;
		digitalinval = digitalin_val_1;
	}
	else if (digitalinid == 2)
	{
		if (DEBUG)
			Serial.println("AXIS 2");
		if (DEBUG)
			Serial.println("digitalin 2");
		digitalinpin = pinConfig.DIGITAL_IN_2;
		digitalinval = digitalin_val_2;
	}
	else if (digitalinid == 3)
	{
		if (DEBUG)
			Serial.println("AXIS 3");
		if (DEBUG)
			Serial.println("digitalin 1");
		digitalinpin = pinConfig.DIGITAL_IN_3;
		digitalinval = digitalin_val_3;
	}

	jsonDocument.clear();
	jsonDocument["digitalinid"] = digitalinid;
	jsonDocument["digitalinval"] = digitalinval;
	jsonDocument["digitalinpin"] = digitalinpin;
	return jsonDocument;
}

void DigitalInController::setup()
{
	Serial.println("Setting Up digitalin");
	/* setup the output nodes and reset them to 0*/
	pinMode(pinConfig.DIGITAL_IN_1, INPUT_PULLDOWN);
	pinMode(pinConfig.DIGITAL_IN_2, INPUT_PULLDOWN);
	pinMode(pinConfig.DIGITAL_IN_3, INPUT_PULLDOWN);
}

void DigitalInController::loop(){
	// readout digital pins one by one
	digitalin_val_1 = digitalRead(pinConfig.DIGITAL_IN_1);
	digitalin_val_2 = digitalRead(pinConfig.DIGITAL_IN_2);
	digitalin_val_3 = digitalRead(pinConfig.DIGITAL_IN_3);


}
