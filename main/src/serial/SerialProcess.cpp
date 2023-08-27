
#include "SerialProcess.h"
#include "../config/ConfigController.h"
#include "../wifi/Endpoints.h"
#include "../analogin/AnalogInController.h"
#include "../scanner/ScannerController.h"
#include "../digitalout/DigitalOutController.h"
#include "../digitalin/DigitalInController.h"
#include "../dac/DacController.h"
#include "../bt/BtController.h"
#include "../wifi/RestApiCallbacks.h"
#include "ModulesToCheck.h"

SerialProcess::SerialProcess(/* args */)
{
}

SerialProcess::~SerialProcess()
{
}



void SerialProcess::loop()
{

	// Config::loop(); // make it sense to call this everyime?
	if (Serial.available())
	{
		String c = Serial.readString();
		const char *s = c.c_str();
		Serial.flush();
		log_i("String s:%s , char:%s", c.c_str(), s);
		cJSON *root = cJSON_Parse(s);
		if (root != NULL)
		{
			cJSON *tasks = cJSON_GetObjectItemCaseSensitive(root, "tasks");
			if (tasks != NULL)
			{

				// {"tasks":[{"task":"/state_get"},{"task":"/state_act", "delay":1000}],"nTimes":2}
				int nTimes = 1;
				// perform the table n-times
				if (cJSON_GetObjectItemCaseSensitive(root, "nTimes")->valueint != NULL)
					nTimes = cJSON_GetObjectItemCaseSensitive(root, "nTimes")->valueint;

				for (int i = 0; i < nTimes; i++)
				{
					log_i("Process tasks");
					cJSON *t = NULL;
					char *string = NULL;
					cJSON_ArrayForEach(t, tasks)
					{
						cJSON *ta = cJSON_GetObjectItemCaseSensitive(t, "task");
						string = cJSON_GetStringValue(ta);
						jsonProcessor(string, t);
					}
				}
			}
			else
			{

				cJSON *string = cJSON_GetObjectItemCaseSensitive(root, "task");
				char *ss = cJSON_GetStringValue(string);
				// log_i("Process task:%s", ss);
				jsonProcessor(ss, root);
			}

			cJSON_Delete(root);
		}
		else
		{
			const char *error_ptr = cJSON_GetErrorPtr();
			if (error_ptr != NULL)
				log_i("error while parsing:%s", error_ptr);
			log_i("Serial input is null");
		}
		c.clear();
	}
}

void SerialProcess::serialize(cJSON *doc)
{
	Serial.println("++");
	if (doc != NULL)
	{
		char *s = cJSON_Print(doc);
		Serial.println(s);
		cJSON_Delete(doc);
		free(s);
	}
	Serial.println("--");
}

void SerialProcess::serialize(int idsuccess)
{
	cJSON *doc = cJSON_CreateObject();
	cJSON *v = cJSON_CreateNumber(idsuccess);
	cJSON_AddItemToObject(doc, "idsuccess", v);
	Serial.println("++");
	char *s = cJSON_Print(doc);
	Serial.println(s);
	cJSON_Delete(doc);
	free(s);
	Serial.println();
	Serial.println("--");
}

void SerialProcess::jsonProcessor(char *task, cJSON *jsonDocument)
{

	// Check if the command gets through
	int moduleAvailable = false;

	// loop through modules with act and get methods
	size_t size = sizeof(modulesToCheck) / sizeof(modulesToCheck[0]);
	for (size_t i = 0; i < size; i++)
	{
		if (moduleController.get(modulesToCheck[i].mod) != nullptr)
		{
			if (strcmp(task, modulesToCheck[i].act) == 0)
			{
				log_i("State act");
				serialize(moduleController.get(modulesToCheck[i].mod)->act(jsonDocument));
				moduleAvailable = true;
			}
			if (strcmp(task, modulesToCheck[i].get) == 0)
			{
				log_i("State get");
				serialize(moduleController.get(modulesToCheck[i].mod)->get(jsonDocument));
				moduleAvailable = true;
			}
		}
	}

	if (strcmp(task, modules_get_endpoint) == 0)
	{
		serialize(moduleController.get());
		moduleAvailable = true;
	}

	if (moduleController.get(AvailableModules::analogJoystick) != nullptr)
	{
		if (strcmp(task, analog_joystick_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::analogJoystick)->get(jsonDocument));
			moduleAvailable = true;
		}
	}

	if (strcmp(task, scanwifi_endpoint) == 0)
	{
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		serialize(w->scan());
		moduleAvailable = true;
	}
	// {"task":"/wifi/scan"}
	if (strcmp(task, connectwifi_endpoint) == 0 && moduleController.get(AvailableModules::wifi) != nullptr)
	{ // {"task":"/wifi/connect","ssid":"Test","PW":"12345678", "AP":false}
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->connect(jsonDocument);
		moduleAvailable = true;
	}
	/*if (task == reset_nv_flash_endpoint)
	{
		RestApi::resetNvFLash();
	}*/
	if (strcmp(task, bt_connect_endpoint) == 0 && moduleController.get(AvailableModules::btcontroller) != nullptr)
	{
		// {"task":"/bt_connect", "mac":"1a:2b:3c:01:01:01", "psx":2}
		char *mac = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(jsonDocument, "mac")); // jsonDocument["mac"];
		int ps = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(jsonDocument, "psx"));	 // jsonDocument["psx"];
		BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
		if (ps == 0)
		{
			bt->setMacAndConnect(mac);
		}
		else
		{
			bt->connectPsxController(mac, ps);
		}
		moduleAvailable = true;
	}
	if (strcmp(task, bt_scan_endpoint) == 0 && moduleController.get(AvailableModules::btcontroller) != nullptr)
	{
		BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
		bt->scanForDevices(jsonDocument);
		moduleAvailable = true;
	}
	// module has not been loaded, so we need at least some error handling/message
	if (!moduleAvailable)
	{
		// we want to state that the command didn't get through
		serialize(0);
	}
}
SerialProcess serial;
