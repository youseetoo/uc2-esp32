#include "DS18b20Controller.h"
#include "OneWire.h";
#include "DallasTemperature.h";

DS18b20Controller::DS18b20Controller(/* args */){};
DS18b20Controller::~DS18b20Controller(){};


void DS18b20Controller::loop() {
	// nothing to do here
}


// Static task function
void temperatureTask(void *pvParameters) {
	for (;;) {
		DS18b20Controller *ds18b20 = (DS18b20Controller *)moduleController.get(AvailableModules::ds18b20);
		ds18b20->mDS18B20->requestTemperatures();
        ds18b20->currentValueCelcius=ds18b20->mDS18B20->getTempCByIndex(0);
        // Sleep for a while
        vTaskDelay(pdMS_TO_TICKS(1000)); // Adjust the delay as needed
    }
}

float DS18b20Controller::readTemperature() {
	return currentValueCelcius;
}


// Custom function accessible by the API
int DS18b20Controller::act(cJSON *ob)
{ 
	// nothing to do here
	return 1;
}

cJSON *DS18b20Controller::get(cJSON *ob)
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

void DS18b20Controller::setup()
{
	if (pinConfig.DS28b20_PIN > -1)
	{

		log_d("Setup DS18b20Controller on pin %i", pinConfig.DS28b20_PIN);
		mOneWire = new OneWire(pinConfig.DS28b20_PIN);
		mDS18B20 = new DallasTemperature(mOneWire);
		mDS18B20->begin();

		// Create the temperature reading task
		xTaskCreate(temperatureTask, "TemperatureTask", pinConfig.TEMPERATURE_TASK_STACKSIZE, this, 1, &temperatureTaskHandle);


	}
}
