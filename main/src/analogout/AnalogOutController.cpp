#include "../../config.h"
#include "AnalogOutController.h"

namespace RestApi
{
	void AnalogOut_act()
    {
		moduleController.get(AvailableModules::analogout)->act(deserialize());
        serialize();
    }

    void AnalogOut_get()
    {
        moduleController.get(AvailableModules::analogout)->get(deserialize());
        serialize();
    }

    void AnalogOut_set()
    {
        moduleController.get(AvailableModules::analogout)->set(deserialize());
        serialize();
    }
}

AnalogOutController::AnalogOutController(){};
AnalogOutController::~AnalogOutController(){};

void AnalogOutController::loop(){}

void AnalogOutController::setup()
{
	Config::getAnalogOutPins(pins);
	Serial.println("Setting Up analogout");
	/* setup the PWM ports and reset them to 0*/
	ledcSetup(PWM_CHANNEL_analogout_1, pwm_frequency, pwm_resolution);
	ledcAttachPin(pins.analogout_PIN_1, PWM_CHANNEL_analogout_1);
	ledcWrite(PWM_CHANNEL_analogout_1, 0);

	ledcSetup(PWM_CHANNEL_analogout_2, pwm_frequency, pwm_resolution);
	ledcAttachPin(pins.analogout_PIN_2, PWM_CHANNEL_analogout_2);
	ledcWrite(PWM_CHANNEL_analogout_2, 0);
}

// Custom function accessible by the API
void AnalogOutController::act(JsonObject  ob)
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
	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["return"] = 1;
}

void AnalogOutController::set(JsonObject  ob)
{
	// here you can set parameters

	int analogoutid = 0;
	if (ob.containsKey("analogoutid"))
	{
		analogoutid = (ob)["analogoutid"];
	}

	int analogoutpin = 0;
	if (ob.containsKey("analogoutpin"))
	{
		int analogoutpin = (ob)["analogoutpin"];
	}

	if (DEBUG)
		Serial.print("analogoutid ");
	Serial.println(analogoutid);
	if (DEBUG)
		Serial.print("analogoutpin ");
	Serial.println(analogoutpin);

	if (analogoutid == 1)
	{
		pins.analogout_PIN_1 = analogoutpin;
		pinMode(pins.analogout_PIN_1, OUTPUT);
		digitalWrite(pins.analogout_PIN_1, LOW);

		/* setup the PWM ports and reset them to 0*/
		ledcSetup(PWM_CHANNEL_analogout_1, pwm_frequency, pwm_resolution);
		ledcAttachPin(pins.analogout_PIN_1, PWM_CHANNEL_analogout_1);
		ledcWrite(PWM_CHANNEL_analogout_1, 0);
	}
	else if (analogoutid == 2)
	{
		pins.analogout_PIN_2 = analogoutpin;
		pinMode(pins.analogout_PIN_2, OUTPUT);
		digitalWrite(pins.analogout_PIN_2, LOW);

		/* setup the PWM ports and reset them to 0*/
		ledcSetup(PWM_CHANNEL_analogout_2, pwm_frequency, pwm_resolution);
		ledcAttachPin(pins.analogout_PIN_2, PWM_CHANNEL_analogout_2);
		ledcWrite(PWM_CHANNEL_analogout_2, 0);
	}
	else if (analogoutid == 3)
	{
		pins.analogout_PIN_3 = analogoutpin;
		pinMode(pins.analogout_PIN_3, OUTPUT);
		digitalWrite(pins.analogout_PIN_3, LOW);

		/* setup the PWM ports and reset them to 0*/
		ledcSetup(PWM_CHANNEL_analogout_3, pwm_frequency, pwm_resolution);
		ledcAttachPin(pins.analogout_PIN_3, PWM_CHANNEL_analogout_3);
		ledcWrite(PWM_CHANNEL_analogout_3, 0);
	}
	Config::setAnalogOutPins(pins);

}

// Custom function accessible by the API
void AnalogOutController::get(JsonObject jsonDocument)
{
	// GET SOME PARAMETERS HERE
	int analogoutid = (*WifiController::getJDoc())["analogoutid"];
	int analogoutpin = 0;
	int analogoutval = 0;

	if (analogoutid == 1)
	{
		if (DEBUG)
			Serial.println("analogout 1");
		analogoutpin = pins.analogout_PIN_1;
		analogoutval = analogout_val_1;
	}
	else if (analogoutid == 2)
	{
		if (DEBUG)
			Serial.println("AXIS 2");
		if (DEBUG)
			Serial.println("analogout 2");
		analogoutpin = pins.analogout_PIN_2;
		analogoutval = analogout_val_2;
	}
	else if (analogoutid == 3)
	{
		if (DEBUG)
			Serial.println("AXIS 3");
		if (DEBUG)
			Serial.println("analogout 1");
		analogoutpin = pins.analogout_PIN_3;
		analogoutval = analogout_val_3;
	}

	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["analogoutid"] = analogoutid;
	(*WifiController::getJDoc())["analogoutval"] = analogoutval;
	(*WifiController::getJDoc())["analogoutpin"] = analogoutpin;
}
