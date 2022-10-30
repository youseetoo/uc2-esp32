#include "../../config.h"
#include "DigitalOutController.h"


// This is for digitaloutout

namespace RestApi
{
	void DigitalOut_act()
	{
		moduleController.get(AvailableModules::digitalout)->act(deserialize());
		serialize();
	}

	void DigitalOut_get()
	{
		moduleController.get(AvailableModules::digitalout)->get(deserialize());
		serialize();
	}

	void DigitalOut_set()
	{
		moduleController.get(AvailableModules::digitalout)->set(deserialize());
		serialize();
	}
}

DigitalOutController::DigitalOutController(/* args */){};
DigitalOutController::~DigitalOutController(){};

// Custom function accessible by the API
void DigitalOutController::act(JsonObject jsonDocument)
{
	// here you can do something
	Serial.println("digitalout_act_fct");
	isBusy = true;
	int triggerdelay = 10;

	int digitaloutid = (jsonDocument)["digitaloutid"];
	int digitaloutval = (jsonDocument)["digitaloutval"];

	if (DEBUG)
	{
		Serial.print("digitaloutid ");
		Serial.println(digitaloutid);
		Serial.print("digitaloutval ");
		Serial.println(digitaloutval);
	}

	if (digitaloutid == 1)
	{
		digitalout_val_1 = digitaloutval;
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(pins.digitalout_PIN_1, HIGH);
			delay(triggerdelay);
			digitalWrite(pins.digitalout_PIN_1, LOW);
		}
		else
		{
			digitalWrite(pins.digitalout_PIN_1, digitalout_val_1);
			Serial.print("digitalout_PIN ");
			Serial.println(pins.digitalout_PIN_1);
		}
	}
	else if (digitaloutid == 2)
	{
		digitalout_val_2 = digitaloutval;
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(pins.digitalout_PIN_2, HIGH);
			delay(triggerdelay);
			digitalWrite(pins.digitalout_PIN_2, LOW);
		}
		else
		{
			digitalWrite(pins.digitalout_PIN_2, digitalout_val_2);
			Serial.print("digitalout_PIN ");
			Serial.println(pins.digitalout_PIN_2);
		}
	}
	else if (digitaloutid == 3)
	{
		digitalout_val_3 = digitaloutval;
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(pins.digitalout_PIN_3, HIGH);
			delay(triggerdelay);
			digitalWrite(pins.digitalout_PIN_3, LOW);
		}
		else
		{
			digitalWrite(pins.digitalout_PIN_3, digitalout_val_3);
			Serial.print("digitalout_PIN ");
			Serial.println(pins.digitalout_PIN_3);
		}
	}
	jsonDocument.clear();
	(jsonDocument)["return"] = 1;
}

void DigitalOutController::set(JsonObject jsonDocument)
{
	// here you can set parameters
	int digitaloutid = (jsonDocument)["digitaloutid"];
	int digitaloutpin = (jsonDocument)["digitaloutpin"];
	if (DEBUG)
		Serial.print("digitaloutid ");
	Serial.println(digitaloutid);
	if (DEBUG)
		Serial.print("digitaloutpin ");
	Serial.println(digitaloutpin);

	if (digitaloutid != NULL and digitaloutpin != NULL)
	{
		if (digitaloutid == 1)
		{
			pins.digitalout_PIN_1 = digitaloutpin;
			pinMode(pins.digitalout_PIN_1, OUTPUT);
			digitalWrite(pins.digitalout_PIN_1, LOW);
		}
		else if (digitaloutid == 2)
		{
			pins.digitalout_PIN_2 = digitaloutpin;
			pinMode(pins.digitalout_PIN_2, OUTPUT);
			digitalWrite(pins.digitalout_PIN_2, LOW);
		}
		else if (digitaloutid == 3)
		{
			pins.digitalout_PIN_3 = digitaloutpin;
			pinMode(pins.digitalout_PIN_3, OUTPUT);
			digitalWrite(pins.digitalout_PIN_3, LOW);
		}
	}
	Config::setDigitalOutPins(pins);
	isBusy = false;
}

// Custom function accessible by the API
void DigitalOutController::get(JsonObject j)
{
	DynamicJsonDocument * jsonDocument = WifiController::getJDoc();
	// GET SOME PARAMETERS HERE
	int digitaloutid = (*jsonDocument)["digitaloutid"];
	int digitaloutpin = 0;
	int digitaloutval = 0;

	if (digitaloutid == 1)
	{
		if (DEBUG)
			Serial.println("digitalout 1");
		digitaloutpin = pins.digitalout_PIN_1;
		digitaloutval = digitalout_val_1;
	}
	else if (digitaloutid == 2)
	{
		if (DEBUG)
			Serial.println("AXIS 2");
		if (DEBUG)
			Serial.println("digitalout 2");
		digitaloutpin = pins.digitalout_PIN_2;
		digitaloutval = digitalout_val_2;
	}
	else if (digitaloutid == 3)
	{
		if (DEBUG)
			Serial.println("AXIS 3");
		if (DEBUG)
			Serial.println("digitalout 1");
		digitaloutpin = pins.digitalout_PIN_3;
		digitaloutval = digitalout_val_3;
	}

	jsonDocument->clear();
	(*jsonDocument)["digitaloutid"] = digitaloutid;
	(*jsonDocument)["digitaloutval"] = digitaloutval;
	(*jsonDocument)["digitaloutpin"] = digitaloutpin;
}

void DigitalOutController::setup()
{
	Config::getDigitalOutPins(pins);
	Serial.println("Setting Up digitalout");
	/* setup the output nodes and reset them to 0*/
	pinMode(pins.digitalout_PIN_1, OUTPUT);

	digitalWrite(pins.digitalout_PIN_1, HIGH);
	delay(50);
	digitalWrite(pins.digitalout_PIN_1, LOW);

	pinMode(pins.digitalout_PIN_2, OUTPUT);
	digitalWrite(pins.digitalout_PIN_2, HIGH);
	delay(50);
	digitalWrite(pins.digitalout_PIN_2, LOW);

	pinMode(pins.digitalout_PIN_3, OUTPUT);
	digitalWrite(pins.digitalout_PIN_3, HIGH);
	delay(50);
	digitalWrite(pins.digitalout_PIN_3, LOW);
}

void DigitalOutController::loop(){}
