#include "../../config.h"
#include "DacController.h"

DacController::DacController(){

};
DacController::~DacController(){

};

namespace RestApi
{
	void Dac_act()
	{
		serialize(moduleController.get(AvailableModules::dac)->act(deserialize()));
	}

	void Dac_get()
	{
		serialize(moduleController.get(AvailableModules::dac)->get(deserialize()));
	}

	void Dac_set()
	{
		serialize(moduleController.get(AvailableModules::dac)->set(deserialize()));
	}
}

void DacController::setup()
{
	Config::getDacPins(pins);
	dacm = new DAC_Module();
	dacm->Setup(DAC_CHANNEL_1, 1000, 50, 0, 0, 2);
	dacm->Setup(DAC_CHANNEL_2, 1000, 50, 0, 0, 2);
	pinMode(pins.dac_fake_1, OUTPUT);
	pinMode(pins.dac_fake_2, OUTPUT);
	frequency = 1;
	(
		drive_galvo,   // Function that should be called
		"drive_galvo", // Name of the task (for debugging)
		1000,		   // Stack size (bytes)
		this,		   // Parameter to pass
		1,			   // Task priority
		NULL		   // Task handle
	);
}

void DacController::loop() {}

// Custom function accessible by the API
DynamicJsonDocument DacController::act(DynamicJsonDocument ob)
{
	// here you can do something

	Serial.println("dac_act_fct");

	// apply default parameters
	// DAC Channel
	dac_channel = DAC_CHANNEL_1;
	if (ob.containsKey("dac_channel"))
	{
		dac_channel = (ob)["dac_channel"];
	}

	// DAC Frequency
	frequency = 1000;
	if (ob.containsKey("frequency"))
	{
		frequency = (ob)["frequency"];
	}

	// DAC offset
	int offset = 0;
	if (ob.containsKey("offset"))
	{
		int offset = (ob)["offset"];
	}

	// DAC amplitude
	int amplitude = 0;
	if (ob.containsKey("amplitude"))
	{
		int amplitude = (ob)["amplitude"];
	}

	// DAC clk_div
	int clk_div = 0;
	if (ob.containsKey("clk_div"))
	{
		int clk_div = (ob)["clk_div"];
	}

	if ((ob)["dac_channel"] == 1)
		dac_channel = DAC_CHANNEL_1;
	else if ((ob)["dac_channel"] == 2)
		dac_channel = DAC_CHANNEL_2;

	// Scale output of a DAC channel using two bit pattern:
	if (amplitude == 0)
		scale = 0;
	else if (amplitude == 1)
		scale = 01;
	else if (amplitude == 2)
		scale = 10;
	else if (amplitude == 3)
		scale = 11;

	if (DEBUG)
	{
		Serial.print("dac_channel ");
		Serial.println(dac_channel);
		Serial.print("frequency ");
		Serial.println(frequency);
		Serial.print("offset ");
		Serial.println(offset);
	}

	if (dac_is_running)
		if (frequency == 0)
		{
			dac_is_running = false;
			dacm->Stop(dac_channel);
			dacWrite(dac_channel, offset);
		}
		else
		{
			dacm->Stop(dac_channel);
			dacm->Setup(dac_channel, clk_div, frequency, scale, phase, invert);
			dac_is_running = true;
		}
	else
	{
		dacm->Setup(dac_channel, clk_div, frequency, scale, phase, invert);
		dacm->dac_offset_set(dac_channel, offset);
	}

	ob.clear();
	ob["return"] = 1;
	return ob;
}

DynamicJsonDocument DacController::set(DynamicJsonDocument ob)
{
	if ((ob).containsKey(keyDACfake1Pin))
	{
		pins.dac_fake_1 = (ob)[keyDACfake1Pin];
		Config::setDacPins(pins);
		setup();
	}
	if ((ob).containsKey(keyDACfake2Pin))
	{
		pins.dac_fake_2 = (ob)[keyDACfake2Pin];
		Config::setDacPins(pins);
		setup();
	}
}

// Custom function accessible by the API
DynamicJsonDocument DacController::get(DynamicJsonDocument jsonDocument)
{
	// GET SOME PARAMETERS HERE
	int dac_variable = 12343;

	jsonDocument.clear();
	jsonDocument["dac_variable"] = dac_variable;
	return jsonDocument;
}

/*
   wrapper for HTTP requests
*/

void DacController::drive_galvo(void *parameter)
{

	DacController *d = (DacController *)parameter;

	while (true)
	{ // infinite loop
		digitalWrite(d->pins.dac_fake_1, HIGH);
		digitalWrite(d->pins.dac_fake_2, HIGH);
		vTaskDelay(d->frequency / portTICK_PERIOD_MS); // pause 1ms
		digitalWrite(d->pins.dac_fake_1, LOW);
		digitalWrite(d->pins.dac_fake_2, LOW);
		vTaskDelay(d->frequency / portTICK_PERIOD_MS); // pause 1ms
	}
}