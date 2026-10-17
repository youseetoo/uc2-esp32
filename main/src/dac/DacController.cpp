#include <PinConfig.h>
#include "DacController.h"
// T3.3: Arduino.h -> uc2_hal shim (log_*/pinMode/digitalWrite/delay/Serial via uc2_hal.h).
#include <uc2_hal.h>
#include "JsonKeys.h"
#include "cJsonTool.h"

// T5.4: the M5Dial + XIAO boards have no DAC. DacController.h only declares the
// dac_channel static under `#if !defined(M5DIAL) && !defined(ESP32S3_MODEL_XIAO)`,
// but this body uses dac_channel throughout. PIO never compiles this file for
// those boards (no -DDAC_CONTROLLER); IDF's uc2_app does, so it must be gated the
// same way here, or the build fails on the undeclared dac_channel.
#ifndef ESP32S3_MODEL_XIAO
#if !defined(M5DIAL)

static const char *TAG = "DacController";

namespace DacController
{

	void setup()
	{
		if (pinConfig.dac_fake_1 < 0 || pinConfig.dac_fake_2 < 0)
		{
			log_i("No DAC pins defined");
			return;
		}
		else if (pinConfig.BUZZER_PIN >= 0)
		{
			log_i("DAC pin used for Buzzer, not initializing DAC output here - use /buzzer_act task to control buzzer output");
			return;
		}
		else
		{
			log_i("DAC pins not defined and no buzzer pin defined - DAC output will not work");
		}

		log_i("Setting up DAC on channel %i, %i", DAC_CHANNEL_1, DAC_CHANNEL_2);

		
		// TODO: need to update this function to match the new framework
		dacm = new DAC_Module();
		// void DAC_Module::Setup(dac_channel_t channel, int clk_div, int frequency, int scale, int phase, int invert) {
		dacm->Setup(DAC_CHANNEL_1, 1, 50, 0, 0, 2);
		dacm->Setup(DAC_CHANNEL_2, 1, 50, 0, 0, 2);
		frequency = 1;

		// make short high/low pulses on the dac channels
		dacm->Setup(DAC_CHANNEL_1, 1, 0, 0, 0, 0); // channel, clk_div, frequency, scale, phase, invert);
		dacWrite(DAC_CHANNEL_1, 255);
		delay(100);
		dacWrite(DAC_CHANNEL_1, 0);
		dacm->Setup(DAC_CHANNEL_2, 1, 0, 0, 0, 0); // clk_div, frequency, scale, phase, invert);
		dacWrite(DAC_CHANNEL_2, 255);
		delay(100);
		dacWrite(DAC_CHANNEL_2, 0);
		
	}

	// Custom function accessible by the API
	int act(cJSON *ob)
	{
		/*
		{"task":"/dac_act", "dac_channel":1, "frequency":0, "offset":100, "amplitude":1, "divider":1, "phase":0, "invert":0, "qid":2}
		{"task":"/dac_act", "dac_channel":1, "frequency":10, "offset":1, "amplitude":1, "divider":0, "phase":0, "invert":1, "qid":2}
		{"task":"/dac_act", "dac_channel":1, "frequency":100, "isFake":1, "qid":2}
		{"task":"/dac_act", "dac_channel":1, "frequency":10, "isFake":1, "qid":2}
		*/

		cJSON *monitor_json = ob;
		int qid = cJsonTool::getJsonInt(ob, "qid");
		if (dacm == nullptr)
			return qid;   // DAC not initialized (setup() not called)
		// here you can do something
		// apply default parameters
		// DAC Channel
		bool isFake = cJsonTool::getJsonInt(ob, "isFake", 0);
		if (isFake)
		{
			cJSON *frequencyj = cJSON_GetObjectItemCaseSensitive(monitor_json, "frequency");
			frequency = frequencyj->valueint;
			xTaskCreatePinnedToCore(drive_galvo, "drive_galvo", 4096, NULL, 1, NULL, 1);
			return qid;
		}
		dac_channel = DAC_CHANNEL_1;
		fake_galvo_running = false;
		cJSON *dac_channelj = cJSON_GetObjectItemCaseSensitive(monitor_json, "dac_channel");
		if (cJSON_IsNumber(dac_channelj))
		{
			dac_channel = (dac_channel_t)dac_channelj->valueint; //(ob)["dac_channel"];
		}

		// only set the DAC value (e.g. analog value 0-255)
		cJSON *dac_value = cJSON_GetObjectItemCaseSensitive(monitor_json, "dac_value");
		if (cJSON_IsNumber(dac_value))
		{ // {"task":"/dac_act", "dac_channel":1, "dac_value":255, "qid":2}
			dacm->Stop(dac_channel);
			dac_is_running = false;
			// set default value for DAC
			dacm->Setup(dac_channel, 1, 0, 0, 0, 0);
			dacWrite(dac_channel, dac_value->valueint);
			return qid;
		}

		// DAC Frequency
		cJSON *frequencyj = cJSON_GetObjectItemCaseSensitive(monitor_json, "frequency");
		if (cJSON_IsNumber(frequencyj))
		{
			frequency = frequencyj->valueint;
		}

		// DAC offset
		int offset = 0;
		cJSON *offsetj = cJSON_GetObjectItemCaseSensitive(monitor_json, "offset");
		if (cJSON_IsNumber(offsetj))
		{
			offset = frequencyj->valueint;
		}

		// DAC phase
		int phase = 0;
		cJSON *phasej = cJSON_GetObjectItemCaseSensitive(monitor_json, "phase");
		if (cJSON_IsNumber(phasej))
		{
			phase = phasej->valueint;
		}

		// DAC invert
		int invert = 2;
		cJSON *invertj = cJSON_GetObjectItemCaseSensitive(monitor_json, "invert");
		if (cJSON_IsNumber(invertj))
		{
			invert = invertj->valueint;
		}

		// DAC amplitude
		int amplitude = 0;
		cJSON *amplitudej = cJSON_GetObjectItemCaseSensitive(monitor_json, "amplitude");
		if (cJSON_IsNumber(amplitudej))
		{
			amplitude = amplitudej->valueint;
		}

		// DAC clk_div
		int clk_div = 0;
		cJSON *clk_divj = cJSON_GetObjectItemCaseSensitive(monitor_json, "divider");
		if (cJSON_IsNumber(clk_divj))
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

		if (dac_is_running)
		{
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
		}
		else
		{
			dacm->Setup(dac_channel, clk_div, frequency, scale, phase, invert);
			dacm->dac_offset_set(dac_channel, offset);
		}

		log_i("DAC Channel %i, Frequency %i, Offset %i, Amplitude %i, Divider %i, Phase %i, Invert %i", dac_channel, frequency, offset, amplitude, clk_div, phase, invert);

		return qid;
	}

	/*
	   wrapper for HTTP requests
	*/

	void drive_galvo(void *parameter)
	{
		// FIXME:_ This is the "Fake" galvo if we cannot access pin 25/26 - should run in background
		Serial.println("Starting fake galvo");
		Serial.println(frequency);
		fake_galvo_running = true;
		pinMode(pinConfig.dac_fake_1, OUTPUT);
		pinMode(pinConfig.dac_fake_2, OUTPUT);

		while (fake_galvo_running)
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
#endif // M5DIAL
#endif // ESP32S3_MODEL_XIAO
