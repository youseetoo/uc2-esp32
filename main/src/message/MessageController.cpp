#include <PinConfig.h>

#include "MessageController.h"
#include "cJsonTool.h"
#include "Arduino.h"
#include "JsonKeys.h"

namespace MessageController
{
	const char * TAG = "MessageController";
	void setup()
	{
		log_d("Setup MessageController");
	}

    void triangle_changed_event(uint8_t pressed)
    {
		sendMesageSerial(1, 1);
    }

	void square_changed_event(uint8_t pressed)
    {
		sendMesageSerial(1, 0);
    }

    // Custom function accessible by the API
	
	int act(cJSON *ob)
	{	// {"message":{"key":"message","value":2}} or 
		// {"task":"/message_act", "message":{"key":"message","value":2}}
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
	cJSON *message = cJSON_GetObjectItem(ob, "message");
	int qid = 0;
	if (message != NULL){
    	qid = cJsonTool::getJsonInt(message, "qid");
		int key = cJsonTool::getJsonInt(message, "key");
		int value = cJsonTool::getJsonInt(message, "value");
		// now we print this data over serial	
		sendMesageSerial(key, value);
	}
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
		cJSON_Delete(json);  // Only delete the top-level object
		Serial.println(ret);
		free(ret);
		Serial.println("--");

	}
}