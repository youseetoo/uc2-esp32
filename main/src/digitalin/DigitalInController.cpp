#include "../../config.h"
#include "DigitalInController.h"


// This is for digitalinout

namespace RestApi
{
	void DigitalIn_act()
	{
		moduleController.get(AvailableModules::digitalin)->act(deserialize());
		serialize();
	}

	void DigitalIn_get()
	{
		moduleController.get(AvailableModules::digitalin)->get(deserialize());
		serialize();
	}

	void DigitalIn_set()
	{
		moduleController.get(AvailableModules::digitalin)->set(deserialize());
		serialize();
	}
}

DigitalInController::DigitalInController(/* args */){};
DigitalInController::~DigitalInController(){};

// Custom function accessible by the API
void DigitalInController::act(JsonObject jsonDocument)
{
	// here you can do something
	Serial.println("digitalin_act_fct");
	jsonDocument.clear();
	(jsonDocument)["return"] = 1;
}

void DigitalInController::set(JsonObject  jsonDocument)
{
	// here you can set parameters
	int digitalinid = (jsonDocument)["digitalinid"];
	int digitalinpin = (jsonDocument)["digitalinpin"];
	if (DEBUG)
		Serial.print("digitalinid ");
	Serial.println(digitalinid);
	if (DEBUG)
		Serial.print("digitalinpin ");
	Serial.println(digitalinpin);

	if (digitalinid != NULL and digitalinpin != NULL)
	{
		if (digitalinid == 1)
		{
			pins.digitalin_PIN_1 = digitalinpin;
			pinMode(pins.digitalin_PIN_1, INPUT_PULLDOWN);  // PULLDOWN
		}
		else if (digitalinid == 2)
		{
			pins.digitalin_PIN_2 = digitalinpin;
			pinMode(pins.digitalin_PIN_2, INPUT_PULLDOWN); // PULLDOWN
		}
		else if (digitalinid == 3)
		{
			pins.digitalin_PIN_3 = digitalinpin;
			pinMode(pins.digitalin_PIN_3, INPUT_PULLDOWN); // PULLDOWN
		}
	}
	Config::setDigitalInPins(pins);
	isBusy = false;
}

// Custom function accessible by the API
void DigitalInController::get(JsonObject  j)
{
	DynamicJsonDocument * jsonDocument = WifiController::getJDoc();
	// GET SOME PARAMETERS HERE
	int digitalinid = (*jsonDocument)["digitalinid"];
	int digitalinpin = 0;
	int digitalinval = 0;

	if (digitalinid == 1)
	{
		if (DEBUG)
			Serial.println("digitalin 1");
		digitalinpin = pins.digitalin_PIN_1;
		digitalinval = digitalin_val_1;
	}
	else if (digitalinid == 2)
	{
		if (DEBUG)
			Serial.println("AXIS 2");
		if (DEBUG)
			Serial.println("digitalin 2");
		digitalinpin = pins.digitalin_PIN_2;
		digitalinval = digitalin_val_2;
	}
	else if (digitalinid == 3)
	{
		if (DEBUG)
			Serial.println("AXIS 3");
		if (DEBUG)
			Serial.println("digitalin 1");
		digitalinpin = pins.digitalin_PIN_3;
		digitalinval = digitalin_val_3;
	}

	jsonDocument->clear();
	(*jsonDocument)["digitalinid"] = digitalinid;
	(*jsonDocument)["digitalinval"] = digitalinval;
	(*jsonDocument)["digitalinpin"] = digitalinpin;
}

void DigitalInController::setup()
{
	Config::getDigitalInPins(pins);
	Serial.println("Setting Up digitalin");
	/* setup the output nodes and reset them to 0*/
	pinMode(pins.digitalin_PIN_1, INPUT_PULLDOWN);
	pinMode(pins.digitalin_PIN_2, INPUT_PULLDOWN);
	pinMode(pins.digitalin_PIN_3, INPUT_PULLDOWN);
}

void DigitalInController::loop(){
	// readout digital pins one by one
	digitalin_val_1 = digitalRead(pins.digitalin_PIN_1);
	digitalin_val_2 = digitalRead(pins.digitalin_PIN_2);
	digitalin_val_3 = digitalRead(pins.digitalin_PIN_3);


}
