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
	}


	// Custom function accessible by the API
	int act(cJSON *ob)
	{
		ESP_LOGI(TAG, "act");
		int qid = cJsonTool::getJsonInt(ob, "qid");
		// serializeJsonPretty(ob, Serial);
		cJSON *Message = cJSON_GetObjectItemCaseSensitive(ob, keyMessage);
		if (Message != NULL)
		{
			MessageModes MessageArrMode = static_cast<MessageModes>(cJSON_GetObjectItemCaseSensitive(Message, keyMessageArrMode)->valueint);
		}
		return qid;
	}

	// Custom function accessible by the API
	// returns json {"Message":{..}}  as qid
	cJSON *get(cJSON *ob)
	{
		cJSON *j = cJSON_CreateObject();
		cJSON *ld = cJSON_CreateObject();
		cJSON_AddItemToObject(j, keyMessage, ld);
		cJsonTool::setJsonInt(ld, keyMessageCount, pinConfig.Message_COUNT);
		cJsonTool::setJsonInt(ld, keyMessagePin, pinConfig.Message_PIN);
		cJsonTool::setJsonInt(ld, key_Message_isOn, isOn);
		return j;
	}

}