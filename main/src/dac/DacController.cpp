#include <PinConfig.h>
#ifdef DAC_CONTROLLER
#include "DacController.h"
#include "Arduino.h"
#include "JsonKeys.h"

namespace DacController
{

	void setup()
	{
		log_i("Setting up DAC on channel %i, %i", DAC_CHANNEL_1, DAC_CHANNEL_2);

		dacm = new DAC_Module();
		// void DAC_Module::Setup(dac_channel_t channel, int clk_div, int frequency, int scale, int phase, int invert) {
		dacm->Setup(DAC_CHANNEL_1, 1, 50, 0, 0, 2);
		dacm->Setup(DAC_CHANNEL_2, 1, 50, 0, 0, 2);
		pinMode(pinConfig.dac_fake_1, OUTPUT);
		pinMode(pinConfig.dac_fake_2, OUTPUT);
		frequency = 1;
	}

	// Custom function accessible by the API
	int act(cJSON *ob)
	{

		cJSON *monitor_json = ob;
		// here you can do something

		Serial.println("dac_act_fct");

		// apply default parameters
		// DAC Channel
		dac_channel = DAC_CHANNEL_1;
		cJSON *dac_channelj = cJSON_GetObjectItemCaseSensitive(monitor_json, "dac_channel");
		if (cJSON_IsNumber(dac_channelj) && dac_channelj->valueint != NULL)
		{
			dac_channel = (dac_channel_t)dac_channelj->valueint; //(ob)["dac_channel"];
		}

		// DAC Frequency
		frequency = 1000;
		cJSON *frequencyj = cJSON_GetObjectItemCaseSensitive(monitor_json, "frequency");
		if (cJSON_IsNumber(frequencyj) && frequencyj->valueint != NULL)
		{
			frequency = frequencyj->valueint;
		}

		// DAC offset
		int offset = 0;
		cJSON *offsetj = cJSON_GetObjectItemCaseSensitive(monitor_json, "offset");
		if (cJSON_IsNumber(offsetj) && offsetj->valueint != NULL)
		{
			offset = frequencyj->valueint;
		}

		// DAC amplitude
		int amplitude = 0;
		cJSON *amplitudej = cJSON_GetObjectItemCaseSensitive(monitor_json, "amplitude");
		if (cJSON_IsNumber(amplitudej) && amplitudej->valueint != NULL)
		{
			amplitude = amplitudej->valueint;
		}

		// DAC clk_div
		int clk_div = 0;
		cJSON *clk_divj = cJSON_GetObjectItemCaseSensitive(monitor_json, "amplitude");
		if (cJSON_IsNumber(clk_divj) && clk_divj->valueint != NULL)
		{
			clk_div = clk_divj->valueint;
		}

		if (dac_channel == 1)
			dac_channel = DAC_CHANNEL_1;
		else if (dac_channel == 2)
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

		// Output debugging information
		Serial.print("dac_channel ");
		Serial.println(dac_channel);
		Serial.print("frequency ");
		Serial.println(frequency);
		Serial.print("offset ");
		Serial.println(offset);

		if (dac_is_running)
			if (frequency == 0)
			{
				Serial.println("Constant value on DAC");
				dac_is_running = false;
				dacm->Stop(dac_channel);
				dacWrite(dac_channel, offset);
			}
			else
			{
				Serial.println("Restarting DAC");
				dacm->Stop(dac_channel);
				dacm->Setup(dac_channel, clk_div, frequency, scale, phase, invert);
				dac_is_running = true;
			}
		else
		{
			Serial.println("Starting DAC");
			dacm->Setup(dac_channel, clk_div, frequency, scale, phase, invert);
			dacm->dac_offset_set(dac_channel, offset);
		}

		return 1;
	}


	/*
	   wrapper for HTTP requests
	*/

	void drive_galvo(void *parameter)
	{
		// FIXME:_ This is the "Fake" galvo if we cannot access pin 25/26 - should run in background

		while (true)
		{ // infinite loop
			digitalWrite(pinConfig.dac_fake_1, HIGH);
			digitalWrite(pinConfig.dac_fake_2, HIGH);
			vTaskDelay(frequency / portTICK_PERIOD_MS); // pause 1ms
			digitalWrite(pinConfig.dac_fake_1, LOW);
			digitalWrite(pinConfig.dac_fake_2, LOW);
			vTaskDelay(frequency / portTICK_PERIOD_MS); // pause 1ms
		}
	}
}
#endif