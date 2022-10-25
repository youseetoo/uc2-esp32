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
		deserialize();
		moduleController.get(AvailableModules::dac)->act();
		serialize();
	}

	void Dac_get()
	{
		deserialize();
		moduleController.get(AvailableModules::dac)->get();
		serialize();
	}

	void Dac_set()
	{
		deserialize();
		moduleController.get(AvailableModules::dac)->set();
		serialize();
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
void DacController::act()
{
	// here you can do something

	Serial.println("dac_act_fct");

	// apply default parameters
	// DAC Channel
	dac_channel = DAC_CHANNEL_1;
	if (WifiController::getJDoc()->containsKey("dac_channel"))
	{
		dac_channel = (*WifiController::getJDoc())["dac_channel"];
	}

	// DAC Frequency
	frequency = 1000;
	if (WifiController::getJDoc()->containsKey("frequency"))
	{
		frequency = (*WifiController::getJDoc())["frequency"];
	}

	// DAC offset
	int offset = 0;
	if (WifiController::getJDoc()->containsKey("offset"))
	{
		int offset = (*WifiController::getJDoc())["offset"];
	}

	// DAC amplitude
	int amplitude = 0;
	if (WifiController::getJDoc()->containsKey("amplitude"))
	{
		int amplitude = (*WifiController::getJDoc())["amplitude"];
	}

	// DAC clk_div
	int clk_div = 0;
	if (WifiController::getJDoc()->containsKey("clk_div"))
	{
		int clk_div = (*WifiController::getJDoc())["clk_div"];
	}

	if ((*WifiController::getJDoc())["dac_channel"] == 1)
		dac_channel = DAC_CHANNEL_1;
	else if ((*WifiController::getJDoc())["dac_channel"] == 2)
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

	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["return"] = 1;
}

void DacController::set()
{
	if ((*WifiController::getJDoc()).containsKey(keyDACfake1Pin))
	{
		pins.dac_fake_1 = (*WifiController::getJDoc())[keyDACfake1Pin];
		Config::setDacPins(pins);
		setup();
	}
	if ((*WifiController::getJDoc()).containsKey(keyDACfake2Pin))
	{
		pins.dac_fake_2 = (*WifiController::getJDoc())[keyDACfake2Pin];
		Config::setDacPins(pins);
		setup();
	}
	WifiController::getJDoc()->clear();
}

// Custom function accessible by the API
void DacController::get()
{
	// GET SOME PARAMETERS HERE
	int dac_variable = 12343;

	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["dac_variable"] = dac_variable;
}

/*
   wrapper for HTTP requests
*/

void DacController::drive_galvo(void *parameter)
{

	DacController *d = (DacController *)parameter;

	while (true)
	{ // infinite loop
		digitaWrite(d->pins.dac_fake_1, HIGH);
		digitaWrite(d->pins.dac_fake_2, HIGH);
		vTaskDelay(d->frequency / portTICK_PERIOD_MS); // pause 1ms
		digitaWrite(d->pins.dac_fake_1, LOW);
		digitaWrite(d->pins.dac_fake_2, LOW);
		vTaskDelay(d->frequency / portTICK_PERIOD_MS); // pause 1ms
	}
}