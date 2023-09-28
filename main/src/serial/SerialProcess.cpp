
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
#include "../state/State.h"

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
				// {"tasks:":[{"task": "/state_get"}, {"task": "/state_act", "delay": 1000}], "nTimes": 3}
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
	// We need to lock the communication in case other modules want to send stuff too
	// This smells more like a thread lock or something?
	Serial.println("++");
	State *state = (State *)moduleController.get(AvailableModules::state);
	state->isSending = true;

	if (doc != NULL)
	{
		char *s = cJSON_Print(doc);
		Serial.println(s);
		cJSON_Delete(doc);
		free(s);
	}
	Serial.println("--");
	state->isSending = false;
}

void SerialProcess::serialize(int qid)
{
	cJSON *doc = cJSON_CreateObject();
	cJSON *v = cJSON_CreateNumber(qid);
	cJSON *n = cJSON_CreateNumber(1);
	cJSON_AddItemToObject(doc, keyQueueID, v);
	cJSON_AddItemToObject(doc, "success", n);
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
	/*
	 enabling/disabling modules
	 */
	if (strcmp(task, modules_get_endpoint) == 0)
	{
		serialize(moduleController.get());
		moduleAvailable = true;
	}

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
			moduleAvailable = true;
		}
		if (strcmp(task, state_get_endpoint) == 0)
		{
			log_i("State get");
			serialize(moduleController.get(AvailableModules::state)->get(jsonDocument));
			moduleAvailable = true;
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
			moduleAvailable = true;
		}
		if (strcmp(task, motor_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::motor)->get(jsonDocument));
			moduleAvailable = true;
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
			moduleAvailable = true;
		}
		if (strcmp(task, home_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::home)->get(jsonDocument));
			moduleAvailable = true;
		}
	}
	/*
	  Encoders
	*/
	if (moduleController.get(AvailableModules::encoder) != nullptr)
	{
		if (strcmp(task, encoder_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::encoder)->act(jsonDocument));
			moduleAvailable = true;
		}
		if (strcmp(task, encoder_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::encoder)->get(jsonDocument));
			moduleAvailable = true;
		}
	}
	/*
	  LinearEncoders
	*/
	if (moduleController.get(AvailableModules::linearencoder) != nullptr)
	{
		if (strcmp(task, linearencoder_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::linearencoder)->act(jsonDocument));
			moduleAvailable = true;
		}
		if (strcmp(task, linearencoder_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::linearencoder)->get(jsonDocument));
			moduleAvailable = true;
		}
	}
	/*
	  Drive DAC
	*/
	if (moduleController.get(AvailableModules::dac) != nullptr)
	{
		if (strcmp(task, dac_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::dac)->act(jsonDocument));
			moduleAvailable = true;
		}
		if (strcmp(task, dac_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::dac)->get(jsonDocument));
			moduleAvailable = true;
		}
	}
	/*
	  Drive Laser
	*/
	if (moduleController.get(AvailableModules::laser) != nullptr)
	{
		if (strcmp(task, laser_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::laser)->act(jsonDocument));
			moduleAvailable = true;
		}
		if (strcmp(task, laser_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::laser)->get(jsonDocument));
			moduleAvailable = true;
		}
	}
	/*
	  Drive analogout
	*/
	if (moduleController.get(AvailableModules::analogout) != nullptr)
	{
		if (strcmp(task, analogout_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::analogout)->act(jsonDocument));
			moduleAvailable = true;
		}

		if (strcmp(task, analogout_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::analogout)->get(jsonDocument));
			moduleAvailable = true;
		}
	}
	/*
	  Drive digitalout
	*/
	if (moduleController.get(AvailableModules::digitalout) != nullptr)
	{
		if (strcmp(task, digitalout_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::digitalout)->act(jsonDocument));
			moduleAvailable = true;
		}
		if (strcmp(task, digitalout_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::digitalout)->get(jsonDocument));
			moduleAvailable = true;
		}
	}
	/*
	  Drive digitalin
	*/
	if (moduleController.get(AvailableModules::digitalin) != nullptr)
	{
		if (strcmp(task, digitalin_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::digitalin)->act(jsonDocument));
			moduleAvailable = true;
		}

		if (strcmp(task, digitalin_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::digitalin)->get(jsonDocument));
			moduleAvailable = true;
		}
	}
	/*
	  Drive LED Matrix
	*/
	if (moduleController.get(AvailableModules::led) != nullptr)
	{
		if (strcmp(task, ledarr_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::led)->act(jsonDocument));
			moduleAvailable = true;
		}

		if (strcmp(task, ledarr_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::led)->get(jsonDocument));
			moduleAvailable = true;
		}
	}

	/*
	  Read the analogin
	*/
	if (moduleController.get(AvailableModules::analogin) != nullptr)
	{
		if (strcmp(task, readanalogin_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::analogin)->act(jsonDocument));
			moduleAvailable = true;
		}

		if (strcmp(task, readanalogin_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::analogin)->get(jsonDocument));
			moduleAvailable = true;
		}
	}

	/*
	  Control PID controller
	*/
	if (moduleController.get(AvailableModules::pid) != nullptr)
	{
		if (strcmp(task, PID_act_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::pid)->act(jsonDocument));
			moduleAvailable = true;
		}

		if (strcmp(task, PID_get_endpoint) == 0)
		{
			serialize(moduleController.get(AvailableModules::pid)->get(jsonDocument));
			moduleAvailable = true;
		}
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
	{ // {"task":"/wifi/scan"}
	}
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
		int qid = 1;
		cJSON *val = cJSON_GetObjectItemCaseSensitive(jsonDocument, keyQueueID);
		if (val == NULL)
		{
			qid = 0;
		}
		if (cJSON_IsNumber(val) and qid >0)
			qid = cJSON_GetNumberValue(val);
		
		serialize(-qid);
	}
}
SerialProcess serial;
