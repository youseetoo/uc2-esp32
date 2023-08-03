
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
			else
			{

				cJSON *string = cJSON_GetObjectItemCaseSensitive(root, "task");
				char *ss = cJSON_GetStringValue(string);
				log_i("Process task:%s", ss);
				jsonProcessor(ss, root);
			}
			
			cJSON_Delete(root);
		}
		else
		{
			log_i("Serial input is null");
		}
		c.clear();
	}
}

void SerialProcess::serialize(cJSON *doc)
{
	Serial.println("++");
	if(doc != NULL)
	{
		char * s = cJSON_Print(doc);
		Serial.println(s);
		cJSON_Delete(doc);
		free(s);
	}
	Serial.println("--");
}

void SerialProcess::serialize(int success)
{
	cJSON *doc = cJSON_CreateObject();
	cJSON *v = cJSON_CreateNumber(success);
	cJSON_AddItemToObject(doc, "success", v);
	Serial.println("++");
	char * s = cJSON_Print(doc);
	Serial.println(s);
	cJSON_Delete(doc);
	free(s);
	Serial.println();
	Serial.println("--");
}

void SerialProcess::jsonProcessor(char *task, cJSON *jsonDocument)
{
	/*
	 enabling/disabling modules
	 */
	if (strcmp(task, modules_get_endpoint) == 0)
		serialize(moduleController.get());

	// Handle BTController
	/*
	if (moduleController.get(AvailableModules::btcontroller) != nullptr)
	{
		if (task == bt_scan_endpoint) // start for Bluetooth Devices
			serialize(moduleController.get(AvailableModules::btcontroller)->Bt_startScan(jsonDocument));
		if (task == bt_paireddevices_endpoint) // get paired devices
			serialize(moduleController.get(AvailableModules::btcontroller)->Bt_getPairedDevices(jsonDocument));
		if (task == bt_connect_endpoint) // connect to device
			serialize(moduleController.get(AvailableModules::btcontroller)->Bt_connect(jsonDocument));
		if (task == bt_remove_endpoint) // remove paired device
			serialize(moduleController.get(AvailableModules::btcontroller)->Bt_remove(jsonDocument));
	}
	*/
	/*
	Return State
	*/
	if (moduleController.get(AvailableModules::state) != nullptr)
	{
		if (strcmp(task, state_act_endpoint) == 0)
		{
			log_i("State act");
			serialize(moduleController.get(AvailableModules::state)->act(jsonDocument));
		}
		if (strcmp(task, state_get_endpoint) == 0)
		{
			log_i("State get");
			serialize(moduleController.get(AvailableModules::state)->get(jsonDocument));
		}
	}
	/*
	  Drive Motors
	*/
	if (moduleController.get(AvailableModules::motor) != nullptr)
	{
		if (strcmp(task, motor_act_endpoint) == 0)
		{
			log_i("process motor act");
			serialize(moduleController.get(AvailableModules::motor)->act(jsonDocument));
		}
		if (strcmp(task, motor_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::motor)->get(jsonDocument));
		}
	}
	/*
	  Home Motors
	*/
	if (moduleController.get(AvailableModules::home) != nullptr)
	{
		if (strcmp(task, home_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::home)->act(jsonDocument));
		}
		if (strcmp(task, home_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::home)->get(jsonDocument));
		}
	}

	/*
	  Drive DAC
	*/
	if (moduleController.get(AvailableModules::dac) != nullptr)
	{
		if (strcmp(task, dac_act_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::dac)->act(jsonDocument));
		if (strcmp(task, dac_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::dac)->get(jsonDocument));
	}
	/*
	  Drive Laser
	*/
	if (moduleController.get(AvailableModules::laser) != nullptr)
	{
		if (strcmp(task, laser_act_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::laser)->act(jsonDocument));
		if (strcmp(task, laser_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::laser)->get(jsonDocument));
	}
	/*
	  Drive analogout
	*/
	if (moduleController.get(AvailableModules::analogout) != nullptr)
	{
		if (strcmp(task, analogout_act_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::analogout)->act(jsonDocument));
		if (strcmp(task, analogout_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::analogout)->get(jsonDocument));
	}
	/*
	  Drive digitalout
	*/
	if (moduleController.get(AvailableModules::digitalout) != nullptr)
	{
		if (strcmp(task, digitalout_act_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::digitalout)->act(jsonDocument));
		if (strcmp(task, digitalout_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::digitalout)->get(jsonDocument));
	}
	/*
	  Drive digitalin
	*/
	if (moduleController.get(AvailableModules::digitalin) != nullptr)
	{
		if (strcmp(task, digitalin_act_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::digitalin)->act(jsonDocument));
		if (strcmp(task, digitalin_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::digitalin)->get(jsonDocument));
	}
	/*
	  Drive LED Matrix
	*/
	if (moduleController.get(AvailableModules::led) != nullptr)
	{
		if (strcmp(task, ledarr_act_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::led)->act(jsonDocument));
		if (strcmp(task, ledarr_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::led)->get(jsonDocument));
	}

	/*
	  Read the analogin
	*/
	if (moduleController.get(AvailableModules::analogin) != nullptr)
	{
		if (strcmp(task, readanalogin_act_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::analogin)->act(jsonDocument));
		if (strcmp(task, readanalogin_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::analogin)->get(jsonDocument));
	}

	/*
	  Control PID controller
	*/
	if (moduleController.get(AvailableModules::pid) != nullptr)
	{
		if (strcmp(task, PID_act_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::pid)->act(jsonDocument));
		if (strcmp(task, PID_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::pid)->get(jsonDocument));
	}

	if (moduleController.get(AvailableModules::analogJoystick) != nullptr)
	{
		if (strcmp(task, analog_joystick_get_endpoint) == 0)
			serialize(moduleController.get(AvailableModules::analogJoystick)->get(jsonDocument));
	}

	if (strcmp(task, scanwifi_endpoint) == 0)
	{ // {"task":"/wifi/scan"}
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		serialize(w->scan());
	}
	if (strcmp(task, connectwifi_endpoint) == 0 && moduleController.get(AvailableModules::wifi) != nullptr)
	{ // {"task":"/wifi/connect","ssid":"Test","PW":"12345678", "AP":false}
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->connect(jsonDocument);
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
	}
	if (strcmp(task, bt_scan_endpoint) == 0 && moduleController.get(AvailableModules::btcontroller) != nullptr)
	{
		BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
		bt->scanForDevices(jsonDocument);
	}
}
SerialProcess serial;
