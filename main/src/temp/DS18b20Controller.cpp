#include "DS18b20Controller.h"

#include "OneWire.h";
#include "DallasTemperature.h";

DS18b20Controller::DS18b20Controller(/* args */){};
DS18b20Controller::~DS18b20Controller(){};

void DS18b20Controller::loop() {
	if ((millis()-lastReading)>(readingPeriod) and pinConfig.DS28b20_PIN > -1)
	{
		currentValueCelcius = readTemperature();
		Serial.println(currentValueCelcius);
		lastReading = millis();
	}
}

float DS18b20Controller::readTemperature(){
	mDS18B20->requestTemperatures();
	currentValueCelcius = mDS18B20->getTempCByIndex(0);
	//Serial.println(currentValueCelcius);
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

	int N_read_avg = 5;
	cJSON *monitor_json = ob;
	// here you can do something
	log_d("ds18b20_get_fct");
	int sensorID = cJSON_GetObjectItemCaseSensitive(monitor_json, key_ds18b20_id)->valueint; //(int)(ob)[key_sensorID];
	if(sensorID == NULL)
		sensorID = 0;

	float analoginValueAvg = 0;
	for (int imeas = 0; imeas < N_read_avg; imeas++)
	{
		analoginValueAvg += readTemperature();
	}
	float returnValue = (float)analoginValueAvg / (float)N_read_avg;
	log_i("Sensor has temp: %f", returnValue);

	cJSON *monitor = cJSON_CreateObject();
	cJSON *analogholder = NULL;
	cJSON *jsonSensorVAL = NULL;
	cJSON *jsonSensorID = NULL;
	jsonSensorVAL = cJSON_CreateNumber(analoginValueAvg);
	jsonSensorID = cJSON_CreateNumber(sensorID);
	cJSON_AddItemToObject(monitor, key_ds18b20_id, jsonSensorID);
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
	}
}
