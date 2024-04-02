#include <PinConfig.h>

#include "MessageController.h"
#include "cJsonTool.h"
#include "Arduino.h"
#include <PinConfig.h>
#include "JsonKeys.h"

namespace MessageController
{
	const char * TAG = "MessageController";
	void setup()
	{
		log_d("Setup MessageController");
	}


	// Custom function accessible by the API
	int act(cJSON *ob)
	{	// {"task":"/message_act", "key":1, "value":0}
		/*
		data lookup:
		{
			key 	|	 meaning 		|	value
			--------------------------------
			1		|	snap image		|	0
			2		|	exposure time	|	0....1000000
			3		|	gain			|	0...100

		*/
		ESP_LOGI(TAG, "act");
		int qid = cJsonTool::getJsonInt(ob, "qid");
		int key = cJsonTool::getJsonInt(ob, "key");
		int value = cJsonTool::getJsonInt(ob, "value");
		// now we print this data over serial	
		sendMesageSerial(key, value);
		return qid;
	}

	// Custom function accessible by the API
	// returns json {"Message":{..}}  as qid
	cJSON *get(cJSON *ob)
	{
		cJSON *j = cJSON_CreateObject();
		cJSON *ld = cJSON_CreateObject();
		cJSON_AddItemToObject(j, "message", ld);
		cJsonTool::setJsonInt(ld, "data", 1);
		return j;
	}

	// home done returns
	//{"message":{...}} thats the qid
	void sendMesageSerial(int key, int value)
	{
		// send home done to client
		log_i("key: %i, value: %i", key, value);
		cJSON *json = cJSON_CreateObject();
		cJSON *message = cJSON_CreateObject();
		cJSON_AddItemToObject(json, "message", message);
		cJSON *jsonkey = cJSON_CreateNumber(key);
		cJSON *jsonvalue = cJSON_CreateNumber(value);
		cJSON_AddItemToObject(message, "key", jsonkey);
		cJSON_AddItemToObject(message, "data", jsonvalue);
		Serial.println("++");
		char *ret = cJSON_Print(json);
		cJSON_Delete(json);
		Serial.println(ret);
		free(ret);
		Serial.println("--");
	}
}