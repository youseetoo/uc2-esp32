#include "DigitalInController.h"

DigitalInController::DigitalInController(/* args */){};
DigitalInController::~DigitalInController(){};

// Custom function accessible by the API
int DigitalInController::act(cJSON *jsonDocument)
{
	// here you can do something
	log_d("digitalin_act_fct");
	return 1;
}

// Custom function accessible by the API
cJSON *DigitalInController::get(cJSON *jsonDocument)
{

	// GET SOME PARAMETERS HERE
	cJSON *monitor_json = jsonDocument;
	int digitalinid = cJSON_GetObjectItemCaseSensitive(monitor_json, "digitalinid")->valueint; // jsonDocument["digitalinid"];
	int digitalinpin = 0;
	int digitalinval = 0;

	// cretae new json document
	// {"task": "/digitalin_get", "digitalinid":1}
	// {"task": "/digitalin_get", "digitalinid":2}
	FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
	if (digitalinid == 1)
	{
		digitalinpin = pinConfig.DIGITAL_IN_1;
		if (pinConfig.I2C_SCL > 0)
		{
			// use the external port extender on UC2v3
			digitalinval = motor->getExternalPinValue(pinConfig.DIGITAL_IN_1);
		}
		else
		{
			digitalinval = digitalRead(pinConfig.DIGITAL_IN_1);
		}
	}
	else if (digitalinid == 2)
	{
		digitalinpin = pinConfig.DIGITAL_IN_2;
		if (pinConfig.I2C_SCL > 0)
		{
			// use the external port extender on UC2v3
			digitalinval = motor->getExternalPinValue(pinConfig.DIGITAL_IN_2);
		}
		else
		{
			digitalinval = digitalRead(pinConfig.DIGITAL_IN_2);
		}
	}
	else if (digitalinid == 3)
	{
		digitalinpin = pinConfig.DIGITAL_IN_3;
		if (pinConfig.I2C_SCL > 0)
		{
			// use the external port extender on UC2v3
			digitalinval = motor->getExternalPinValue(pinConfig.DIGITAL_IN_3);
		}
		else
		{
			digitalinval = digitalRead(pinConfig.DIGITAL_IN_3);
		}
	}
	monitor_json = cJSON_CreateObject();
	cJSON *id = cJSON_CreateNumber(digitalinid);
	cJSON_AddItemToObject(monitor_json, "digitalinid", id);
	cJSON *val = cJSON_CreateNumber(digitalinval);
	cJSON_AddItemToObject(monitor_json, "digitalinval", val);
	cJSON *pin = cJSON_CreateNumber(digitalinpin);
	cJSON_AddItemToObject(monitor_json, "digitalinpin", pin);
	return monitor_json;
}

void DigitalInController::setup()
{

	log_i("Setting Up digitalin");
	if (pinConfig.I2C_SCL < 0)
	{
		/* setup the output nodes and reset them to 0*/
		log_i("DigitalIn 1: %i", pinConfig.DIGITAL_IN_1);
		pinMode(pinConfig.DIGITAL_IN_1, INPUT_PULLDOWN);
		log_i("DigitalIn 2: %i", pinConfig.DIGITAL_IN_2);
		pinMode(pinConfig.DIGITAL_IN_2, INPUT_PULLDOWN);
		log_i("DigitalIn 3: %i", pinConfig.DIGITAL_IN_3);
		pinMode(pinConfig.DIGITAL_IN_3, INPUT_PULLDOWN);
	}
}

void DigitalInController::loop()
{

	// FIXME: Never reaches this position..

	if (pinConfig.I2C_SCL < 0)
	{
		// readout digital pins one by one
		digitalin_val_1 = digitalRead(pinConfig.DIGITAL_IN_1);
		digitalin_val_2 = digitalRead(pinConfig.DIGITAL_IN_2);
		digitalin_val_3 = digitalRead(pinConfig.DIGITAL_IN_3);
	}
	else
	{
		// read values from the TCA input register
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		digitalin_val_1 = motor->getExternalPinValue(pinConfig.DIGITAL_IN_1);
		digitalin_val_2 = motor->getExternalPinValue(pinConfig.DIGITAL_IN_2);
		digitalin_val_3 = motor->getExternalPinValue(pinConfig.DIGITAL_IN_3);
	}
	// log_i("digitalin_val_1: %i, digitalin_val_2: %i, digitalin_val_3: %i", digitalin_val_1, digitalin_val_2, digitalin_val_3);
}
