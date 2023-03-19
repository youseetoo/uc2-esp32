#include "AnalogOutController.h"

namespace RestApi
{
	void AnalogOut_act()
    {
		serialize(moduleController.get(AvailableModules::analogout)->act(deserialize()));
    }

    void AnalogOut_get()
    {
        serialize(moduleController.get(AvailableModules::analogout)->get(deserialize()));
    }

}

AnalogOutController::AnalogOutController(){};
AnalogOutController::~AnalogOutController(){};

void AnalogOutController::loop(){}

void AnalogOutController::setup()
{
	log_d("Setup AnalogOutController");
	/* setup the PWM ports and reset them to 0*/
	ledcSetup(PWM_CHANNEL_analogout_1, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.analogout_PIN_1, PWM_CHANNEL_analogout_1);
	ledcWrite(PWM_CHANNEL_analogout_1, 0);

	ledcSetup(PWM_CHANNEL_analogout_2, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.analogout_PIN_2, PWM_CHANNEL_analogout_2);
	ledcWrite(PWM_CHANNEL_analogout_2, 0);
}

// Custom function accessible by the API
int AnalogOutController::act(DynamicJsonDocument  ob)
{
	// here you can do something
	Serial.println("analogout_act_fct");

	int analogoutid = (ob)["analogoutid"];
	int analogoutval = (ob)["analogoutval"];

	if (DEBUG)
	{
		Serial.print("analogoutid ");
		Serial.println(analogoutid);
		Serial.print("analogoutval ");
		Serial.println(analogoutval);
	}

	if (analogoutid == 1)
	{
		analogout_val_1 = analogoutval;
		ledcWrite(PWM_CHANNEL_analogout_1, analogoutval);
	}
	else if (analogoutid == 2)
	{
		analogout_val_2 = analogoutval;
		ledcWrite(PWM_CHANNEL_analogout_2, analogoutval);
	}
	else if (analogoutid == 3)
	{
		analogout_val_3 = analogoutval;
		ledcWrite(PWM_CHANNEL_analogout_3, analogoutval);
	}
	return 1;
}

// Custom function accessible by the API
DynamicJsonDocument AnalogOutController::get(DynamicJsonDocument jsonDocument)
{
	// GET SOME PARAMETERS HERE
	int analogoutid = jsonDocument["analogoutid"];
	int analogoutpin = 0;
	int analogoutval = 0;

	if (analogoutid == 1)
	{
		if (DEBUG)
			Serial.println("analogout 1");
		analogoutpin = pinConfig.analogout_PIN_1;
		analogoutval = analogout_val_1;
	}
	else if (analogoutid == 2)
	{
		if (DEBUG)
			Serial.println("AXIS 2");
		if (DEBUG)
			Serial.println("analogout 2");
		analogoutpin = pinConfig.analogout_PIN_2;
		analogoutval = analogout_val_2;
	}
	else if (analogoutid == 3)
	{
		if (DEBUG)
			Serial.println("AXIS 3");
		if (DEBUG)
			Serial.println("analogout 1");
		analogoutpin = pinConfig.analogout_PIN_3;
		analogoutval = analogout_val_3;
	}

	jsonDocument.clear();
	jsonDocument["analogoutid"] = analogoutid;
	jsonDocument["analogoutval"] = analogoutval;
	jsonDocument["analogoutpin"] = analogoutpin;
	return jsonDocument;
}
