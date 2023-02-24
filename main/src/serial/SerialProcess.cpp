
#include "SerialProcess.h"

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
		String s = Serial.readString();
		int si = s.length() * 8;
		DynamicJsonDocument jsonDocument(si);
		DeserializationError error = deserializeJson(jsonDocument, s);
		// free(Serial);
		if (error)
		{
			Serial.print(F("deserializeJson() failed: "));
			Serial.println(error.f_str());
			return;
		}
		Serial.flush();
		if (jsonDocument.containsKey("tasks"))
		{
			log_i("task to process:%i", jsonDocument["tasks"].size());
			for (int i = 0; i < jsonDocument["tasks"].size(); i++)
			{
				String task_s = jsonDocument["tasks"][i]["task"];
				JsonObject doc = jsonDocument["tasks"][i].as<JsonObject>();
				jsonProcessor(task_s, doc);
			}
		}
		else
		{
			log_i("process single task");
			String task_s = jsonDocument["task"];
			JsonObject ob = jsonDocument.as<JsonObject>();
			jsonProcessor(task_s, ob);
		}
	}
}

void SerialProcess::serialize(DynamicJsonDocument doc)
{
	Serial.println("++");
	serializeJson(doc, Serial);
	Serial.println();
	Serial.println("--");
	doc.clear();
	doc.garbageCollect();
}

void SerialProcess::serialize(int success)
{
	DynamicJsonDocument doc(4096); //StaticJsonDocument<16> doc;
	doc["success"] = success;
	Serial.println("++");
	serializeJson(doc, Serial);
	Serial.println();
	Serial.println("--");
	doc.clear();
	doc.garbageCollect();
}

void SerialProcess::jsonProcessor(String task, JsonObject jsonDocument)
{
	/*
	 enabling/disabling modules
	 */
	if (task == modules_set_endpoint)
		serialize(moduleController.set(jsonDocument));
	if (task == modules_get_endpoint)
		serialize(moduleController.get());

	
	//Handle BTController
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
		if (task ==  state_act_endpoint)
		{
			serialize(moduleController.get(AvailableModules::state)->act(jsonDocument));
		}
		if (task == state_set_endpoint)
		{
			serialize(moduleController.get(AvailableModules::state)->set(jsonDocument));
		}
		if (task == state_get_endpoint)
		{
			serialize(moduleController.get(AvailableModules::state)->get(jsonDocument));
		}
	}
	/*
	  Drive Motors
	*/
	if (moduleController.get(AvailableModules::motor) != nullptr)
	{
		if (task == motor_act_endpoint)
		{
			serialize(moduleController.get(AvailableModules::motor)->act(jsonDocument));
		}
		if (task == motor_set_endpoint)
		{
			serialize(moduleController.get(AvailableModules::motor)->set(jsonDocument));
		}
		if (task == motor_get_endpoint)
		{
			serialize(moduleController.get(AvailableModules::motor)->get(jsonDocument));
		}
	}
	/*
	  Home Motors
	*/
	if (moduleController.get(AvailableModules::home) != nullptr)
	{
		if (task == home_act_endpoint)
		{
			serialize(moduleController.get(AvailableModules::home)->act(jsonDocument));
		}
		if (task == home_set_endpoint)
		{
			serialize(moduleController.get(AvailableModules::home)->set(jsonDocument));
		}
		if (task == home_get_endpoint)
		{
			serialize(moduleController.get(AvailableModules::home)->get(jsonDocument));
		}
	}

	/*
	  Drive DAC
	*/
	if (moduleController.get(AvailableModules::dac) != nullptr)
	{
		if (task == dac_act_endpoint)
			serialize(moduleController.get(AvailableModules::dac)->act(jsonDocument));
		if (task == dac_set_endpoint)
			serialize(moduleController.get(AvailableModules::dac)->set(jsonDocument));
		if (task == dac_get_endpoint)
			serialize(moduleController.get(AvailableModules::dac)->get(jsonDocument));
	}
	/*
	  Drive Laser
	*/
	if (moduleController.get(AvailableModules::laser) != nullptr)
	{
		if (task == laser_act_endpoint)
			serialize(moduleController.get(AvailableModules::laser)->act(jsonDocument));
		if (task == laser_set_endpoint)
			serialize(moduleController.get(AvailableModules::laser)->set(jsonDocument));
		if (task == laser_get_endpoint)
			serialize(moduleController.get(AvailableModules::laser)->get(jsonDocument));
	}
	/*
	  Drive analogout
	*/
	if (moduleController.get(AvailableModules::analogout) != nullptr)
	{
		if (task == analogout_act_endpoint)
			serialize(moduleController.get(AvailableModules::analogout)->act(jsonDocument));
		if (task == analogout_set_endpoint)
			serialize(moduleController.get(AvailableModules::analogout)->set(jsonDocument));
		if (task == analogout_get_endpoint)
			serialize(moduleController.get(AvailableModules::analogout)->get(jsonDocument));
	}
	/*
	  Drive digitalout
	*/
	if (moduleController.get(AvailableModules::digitalout) != nullptr)
	{
		if (task == digitalout_act_endpoint)
			serialize(moduleController.get(AvailableModules::digitalout)->act(jsonDocument));
		if (task == digitalout_set_endpoint)
			serialize(moduleController.get(AvailableModules::digitalout)->set(jsonDocument));
		if (task == digitalout_get_endpoint)
			serialize(moduleController.get(AvailableModules::digitalout)->get(jsonDocument));
	}
	/*
	  Drive digitalin
	*/
	if (moduleController.get(AvailableModules::digitalin) != nullptr)
	{
		if (task == digitalin_act_endpoint)
			serialize(moduleController.get(AvailableModules::digitalin)->act(jsonDocument));
		if (task == digitalin_set_endpoint)
			serialize(moduleController.get(AvailableModules::digitalin)->set(jsonDocument));
		if (task == digitalin_get_endpoint)
			serialize(moduleController.get(AvailableModules::digitalin)->get(jsonDocument));
	}
	/*
	  Drive LED Matrix
	*/
	if (moduleController.get(AvailableModules::led) != nullptr)
	{
		if (task == ledarr_act_endpoint)
			serialize(moduleController.get(AvailableModules::led)->act(jsonDocument));
		if (task == ledarr_set_endpoint)
			serialize(moduleController.get(AvailableModules::led)->set(jsonDocument));
		if (task == ledarr_get_endpoint)
			serialize(moduleController.get(AvailableModules::led)->get(jsonDocument));
	}

	/*
	  Read the analogin
	*/
	if (moduleController.get(AvailableModules::analogin) != nullptr)
	{
		if (task == readanalogin_act_endpoint)
			serialize(moduleController.get(AvailableModules::analogin)->act(jsonDocument));
		if (task == readanalogin_set_endpoint)
			serialize(moduleController.get(AvailableModules::analogin)->set(jsonDocument));
		if (task == readanalogin_get_endpoint)
			serialize(moduleController.get(AvailableModules::analogin)->get(jsonDocument));
	}

	/*
	  Control PID controller
	*/
	if (moduleController.get(AvailableModules::pid) != nullptr)
	{
		if (task == PID_act_endpoint)
			serialize(moduleController.get(AvailableModules::pid)->act(jsonDocument));
		if (task == PID_set_endpoint)
			serialize(moduleController.get(AvailableModules::pid)->set(jsonDocument));
		if (task == PID_get_endpoint)
			serialize(moduleController.get(AvailableModules::pid)->get(jsonDocument));
	}

	if (moduleController.get(AvailableModules::analogJoystick) != nullptr)
	{
		if (task == analog_joystick_set_endpoint)
			serialize(moduleController.get(AvailableModules::analogJoystick)->set(jsonDocument));
		if (task == analog_joystick_get_endpoint)
			serialize(moduleController.get(AvailableModules::analogJoystick)->get(jsonDocument));
	}

	if (task == scanwifi_endpoint)
	{	// {"task":"/wifi/scan"}
		serialize(RestApi::scanWifi());
	}
	if (task == connectwifi_endpoint)
	{ 	// {"task":"/wifi/connect","ssid":"Test","PW":"12345678", "AP":false}
		WifiController::connect(jsonDocument);
	}
	if (task == reset_nv_flash_endpoint)
	{
		RestApi::resetNvFLash();
	}
	if (task == bt_connect_endpoint)
	{
		// {"task":"/bt_connect", "mac":"1a:2b:3c:01:01:01", "psx":2}
		String mac = jsonDocument["mac"];
		int ps = jsonDocument["psx"];

		if (ps == 0)
		{
			BtController::setMacAndConnect(mac);
		}
		else
		{
			BtController::connectPsxController(mac, ps);
		}
	}
	if (task == bt_scan_endpoint)
	{
		BtController::scanForDevices(jsonDocument);
	}

	jsonDocument.clear();


}
SerialProcess serial;
