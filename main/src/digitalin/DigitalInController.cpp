#include "../../config.h"
#include "DigitalInController.h"


// This is for digitalinout

namespace RestApi
{
	void DigitalIn_act()
	{
		deserialize();
		moduleController.get(AvailableModules::digitalin)->act();
		serialize();
	}

	void DigitalIn_get()
	{
		deserialize();
		moduleController.get(AvailableModules::digitalin)->get();
		serialize();
	}

	void DigitalIn_set()
	{
		deserialize();
		moduleController.get(AvailableModules::digitalin)->set();
		serialize();
	}
}

DigitalInController::DigitalInController(/* args */){};
DigitalInController::~DigitalInController(){};

// Custom function accessible by the API
void DigitalInController::act()
{
	DynamicJsonDocument * jsonDocument = WifiController::getJDoc();
	// here you can do something
	Serial.println("digitalin_act_fct");
	jsonDocument->clear();
	(*jsonDocument)["return"] = 1;
}

void DigitalInController::set()
{
	// here you can set parameters
	DynamicJsonDocument * jsonDocument = WifiController::getJDoc();
	int digitalinid = (*jsonDocument)["digitalinid"];
	int digitalinpin = (*jsonDocument)["digitalinpin"];
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
			pinMode(pins.digitalin_PIN_1, OUTPUT);
			digitalWrite(pins.digitalin_PIN_1, LOW);
		}
		else if (digitalinid == 2)
		{
			pins.digitalin_PIN_2 = digitalinpin;
			pinMode(pins.digitalin_PIN_2, OUTPUT);
			digitalWrite(pins.digitalin_PIN_2, LOW);
		}
		else if (digitalinid == 3)
		{
			pins.digitalin_PIN_3 = digitalinpin;
			pinMode(pins.digitalin_PIN_3, OUTPUT);
			digitalWrite(pins.digitalin_PIN_3, LOW);
		}
	}
	Config::setDigitalInPins(pins);
	isBusy = false;
	jsonDocument->clear();
	(*jsonDocument)["return"] = 1;
}

// Custom function accessible by the API
void DigitalInController::get()
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
	pinMode(pins.digitalin_PIN_1, INPUT);
	pinMode(pins.digitalin_PIN_2, INPUT);
	pinMode(pins.digitalin_PIN_3, INPUT);
}

void DigitalInController::loop(){

/*
	digitalRead()
	*/
}
