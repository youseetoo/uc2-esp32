#include "DigitalOutController.h"


// This is for digitaloutout

namespace RestApi
{
	void DigitalOut_act()
	{
		serialize(moduleController.get(AvailableModules::digitalout)->act(deserialize()));
	}

	void DigitalOut_get()
	{
		serialize(moduleController.get(AvailableModules::digitalout)->get(deserialize()));
	}
}

DigitalOutController::DigitalOutController(/* args */){};
DigitalOutController::~DigitalOutController(){};

// Custom function accessible by the API
int DigitalOutController::act(DynamicJsonDocument jsonDocument)
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
			digitalWrite(pinConfig.DIGITAL_OUT_1, HIGH);
			delay(triggerdelay);
			digitalWrite(pinConfig.DIGITAL_OUT_1, LOW);
		}
		else
		{
			digitalWrite(pinConfig.DIGITAL_OUT_1, digitalout_val_1);
			Serial.print("digitalout_PIN ");
			Serial.println(pinConfig.DIGITAL_OUT_1);
		}
	}
	else if (digitaloutid == 2)
	{
		digitalout_val_2 = digitaloutval;
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(pinConfig.DIGITAL_OUT_2, HIGH);
			delay(triggerdelay);
			digitalWrite(pinConfig.DIGITAL_OUT_2, LOW);
		}
		else
		{
			digitalWrite(pinConfig.DIGITAL_OUT_2, digitalout_val_2);
			Serial.print("digitalout_PIN ");
			Serial.println(pinConfig.DIGITAL_OUT_2);
		}
	}
	else if (digitaloutid == 3)
	{
		digitalout_val_3 = digitaloutval;
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(pinConfig.DIGITAL_OUT_3, HIGH);
			delay(triggerdelay);
			digitalWrite(pinConfig.DIGITAL_OUT_3, LOW);
		}
		else
		{
			digitalWrite(pinConfig.DIGITAL_OUT_3, digitalout_val_3);
			Serial.print("digitalout_PIN ");
			Serial.println(pinConfig.DIGITAL_OUT_3);
		}
	}
	return 1;
}

// Custom function accessible by the API
DynamicJsonDocument DigitalOutController::get(DynamicJsonDocument jsonDocument)
{
	// GET SOME PARAMETERS HERE
	int digitaloutid = jsonDocument["digitaloutid"];
	int digitaloutpin = 0;
	int digitaloutval = 0;

	if (digitaloutid == 1)
	{
		if (DEBUG)
			Serial.println("digitalout 1");
		digitaloutpin = pinConfig.DIGITAL_OUT_1;
		digitaloutval = digitalout_val_1;
	}
	else if (digitaloutid == 2)
	{
		if (DEBUG)
			Serial.println("AXIS 2");
		if (DEBUG)
			Serial.println("digitalout 2");
		digitaloutpin = pinConfig.DIGITAL_OUT_2;
		digitaloutval = digitalout_val_2;
	}
	else if (digitaloutid == 3)
	{
		if (DEBUG)
			Serial.println("AXIS 3");
		if (DEBUG)
			Serial.println("digitalout 1");
		digitaloutpin = pinConfig.DIGITAL_OUT_3;
		digitaloutval = digitalout_val_3;
	}

	jsonDocument.clear();
	jsonDocument["digitaloutid"] = digitaloutid;
	jsonDocument["digitaloutval"] = digitaloutval;
	jsonDocument["digitaloutpin"] = digitaloutpin;
	return jsonDocument;
}

void DigitalOutController::setup()
{
	Serial.println("Setting Up digitalout");
	/* setup the output nodes and reset them to 0*/
	pinMode(pinConfig.DIGITAL_OUT_1, OUTPUT);

	digitalWrite(pinConfig.DIGITAL_OUT_1, HIGH);
	delay(50);
	digitalWrite(pinConfig.DIGITAL_OUT_1, LOW);

	pinMode(pinConfig.DIGITAL_OUT_2, OUTPUT);
	digitalWrite(pinConfig.DIGITAL_OUT_2, HIGH);
	delay(50);
	digitalWrite(pinConfig.DIGITAL_OUT_2, LOW);

	pinMode(pinConfig.DIGITAL_OUT_3, OUTPUT);
	digitalWrite(pinConfig.DIGITAL_OUT_3, HIGH);
	delay(50);
	digitalWrite(pinConfig.DIGITAL_OUT_3, LOW);
}

void DigitalOutController::loop(){}
