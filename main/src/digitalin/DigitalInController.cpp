#include "DigitalInController.h"
#include "../../pindef_UC2_2.h"

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

	void DigitalIn_set()
	{
		serialize(moduleController.get(AvailableModules::digitalin)->set(deserialize()));
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

int DigitalInController::set(DynamicJsonDocument jsonDocument)
{
	// here you can set parameters
	int digitalinid = (jsonDocument)["digitalinid"];
	int digitalinpin = (jsonDocument)["digitalinpin"];

	if (digitalinid != 0 and digitalinpin != 0)
	{
		if (digitalinid == 1)
		{
			pins.digitalin_PIN_1 = digitalinpin;
			pinMode(pins.digitalin_PIN_1, INPUT_PULLDOWN); // PULLDOWN
			log_i("Setting digitalin_PIN_1: %i", digitalinpin);
		}
		else if (digitalinid == 2)
		{
			pins.digitalin_PIN_2 = digitalinpin;
			pinMode(pins.digitalin_PIN_2, INPUT_PULLDOWN); // PULLDOWN
			log_i("Setting digitalin_PIN_2: %i", digitalinpin);
		}
		else if (digitalinid == 3)
		{
			pins.digitalin_PIN_3 = digitalinpin;
			pinMode(pins.digitalin_PIN_3, INPUT_PULLDOWN); // PULLDOWN
			log_i("Setting digitalin_PIN_3: %i", digitalinpin);
		}
	}
	Config::setDigitalInPins(pins);
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
		digitalinpin = pins.digitalin_PIN_1;
		digitalinval = digitalRead(pins.digitalin_PIN_1);
	}
	else if (digitalinid == 2)
	{
		digitalinpin = pins.digitalin_PIN_2;
		digitalinval = digitalRead(pins.digitalin_PIN_2);
	}
	else if (digitalinid == 3)
	{
		digitalinpin = pins.digitalin_PIN_3;
		digitalinval = digitalRead(pins.digitalin_PIN_3);
	}

	doc["digitalinid"] = digitalinid;
	doc["digitalinval"] = digitalinval;
	doc["digitalinpin"] = digitalinpin;
	return doc;
}

void DigitalInController::setup()
{
	Config::getDigitalInPins(pins);
	Serial.println("Setting Up digitalin");
	/* setup the output nodes and reset them to 0*/

	/* Input 1 */
	if (not pins.digitalin_PIN_1)
	{
		pins.digitalin_PIN_1 = PIN_DEF_END_X;
	}
	pinMode(pins.digitalin_PIN_1, INPUT_PULLDOWN);
	log_i("Setting digitalin_PIN_1: %i, value: %i", pins.digitalin_PIN_1, digitalRead(pins.digitalin_PIN_1));
	Serial.println(pins.digitalin_PIN_1);
	
	/* Input 2 */
	if (not pins.digitalin_PIN_2)
	{
		pins.digitalin_PIN_2 = PIN_DEF_END_Y;
	}
	pinMode(pins.digitalin_PIN_2, INPUT_PULLDOWN);
	log_i("Setting digitalin_PIN_2: %i, value: %i", pins.digitalin_PIN_2, digitalRead(pins.digitalin_PIN_2));

	/* Input 3 */
	if (not pins.digitalin_PIN_3)
	{
		pins.digitalin_PIN_3 = PIN_DEF_END_Z;
	}
	pinMode(pins.digitalin_PIN_3, INPUT_PULLDOWN);
	log_i("Setting digitalin_PIN_3: %i, value: %i", pins.digitalin_PIN_3, digitalRead(pins.digitalin_PIN_3));

	Config::setDigitalInPins(pins); // save the pins to the config
}

void DigitalInController::loop()
{

	//FIXME: Never reaches this position..
	
	// readout digital pins one by one
	digitalin_val_1 = digitalRead(pins.digitalin_PIN_1);
	digitalin_val_2 = digitalRead(pins.digitalin_PIN_2);
	digitalin_val_3 = digitalRead(pins.digitalin_PIN_3);
	log_i("digitalin_val_1: %i, digitalin_val_2: %i, digitalin_val_3: %i", digitalin_val_1, digitalin_val_2, digitalin_val_3);
}
