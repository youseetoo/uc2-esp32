#include "DS18b20Controller.h"
#include "OneWire.h"
#include "DallasTemperature.h"
#include "JsonKeys.h"
#include "PinConfig.h"

namespace DS18b20Controller
{
	bool DEBUG = false;

	long lastReading = 0;
	float readingPeriod = 1000; // ms
	float currentValueCelcius = 0.0;

	bool isRequestingTemperature = false;
	long requestStartTime = 0;
	const long MAX_WAIT = 1000; // ms
	DeviceAddress *mDS18B20Addresses;
	OneWire *mOneWire;
	DallasTemperature *mDS18B20;
	TaskHandle_t temperatureTaskHandle;

	float getCurrentValueCelcius()
	{
		return currentValueCelcius;
	}
	// Static task function
	void temperatureTask(void *pvParameters)
	{
		for (;;)
		{
			mDS18B20->requestTemperatures();
			currentValueCelcius = mDS18B20->getTempCByIndex(0);
			// Sleep for a while
			vTaskDelay(pdMS_TO_TICKS(1000)); // Adjust the delay as needed
		}
	}

	float readTemperature()
	{
		return currentValueCelcius;
	}

	// Custom function accessible by the API
	int act(cJSON *ob)
	{
		// nothing to do here
		return 1;
	}

	cJSON *get(cJSON *ob)
	{
		// {"task": "/ds18b20_get"}
		cJSON *monitor_json = ob;
		// here you can do something
		float returnValue = readTemperature();
		log_i("Sensor has temp: %f", returnValue);

		cJSON *monitor = cJSON_CreateObject();
		cJSON *jsonSensorVAL = NULL;
		jsonSensorVAL = cJSON_CreateNumber(returnValue);
		cJSON_AddItemToObject(monitor, key_ds18b20_val, jsonSensorVAL);
		return monitor;
	}

	void setup()
	{
		if (pinConfig.DS28b20_PIN > -1)
		{

			log_d("Setup DS18b20Controller on pin %i", pinConfig.DS28b20_PIN);
			mOneWire = new OneWire(pinConfig.DS28b20_PIN);
			mDS18B20 = new DallasTemperature(mOneWire);
			mDS18B20->begin();

			// Create the temperature reading task
			xTaskCreate(temperatureTask, "TemperatureTask", pinConfig.TEMPERATURE_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, &temperatureTaskHandle);
		}
	}
}
