// T4.1: C++ island — the vendored OneWire + DallasTemperature libs
// (onewire_ds18b20) are C++; the public API is pure C (ds18b20_*, see
// DS18b20Controller.h).
#include "DS18b20Controller.h"
#include "OneWire.h"
#include "DallasTemperature.h"
// JsonKeys.h dropped (Arduino/pgmspace): key_ds18b20_val == "sensorval".
#include "PinConfig.h"
#include "cJsonTool.h"

static const char *TAG = "DS18b20";

namespace
{
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
} // namespace

extern "C" float ds18b20_read_temperature(void)
{
	return currentValueCelcius;
}

// Custom function accessible by the API
extern "C" int ds18b20_act(cJSON *ob)
{
	// nothing to do here
	int qid = cjson_tool_get_int(ob, "qid", 0);
	return qid;
}

extern "C" cJSON *ds18b20_get(cJSON *ob)
{
	// {"task": "/ds18b20_get"}
	cJSON *monitor_json = ob;
	// here you can do something
	float returnValue = ds18b20_read_temperature();
	log_i("Sensor has temp: %f", returnValue);

	cJSON *monitor = cJSON_CreateObject();
	cJSON *jsonSensorVAL = NULL;
	jsonSensorVAL = cJSON_CreateNumber(returnValue);
	cJSON_AddItemToObject(monitor, "sensorval", jsonSensorVAL);
	return monitor;
}

extern "C" float ds18b20_get_current_value_celcius(void)
{
	return currentValueCelcius;
}

extern "C" void ds18b20_setup(void)
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
