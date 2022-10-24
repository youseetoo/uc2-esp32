#include "../../config.h"

#include "SerialProcess.h"

SerialProcess::SerialProcess(/* args */)
{
}

SerialProcess::~SerialProcess()
{
}

void SerialProcess::loop(DynamicJsonDocument *jsonDocument)
{
	// Config::loop(); // make it sense to call this everyime?
	if (Serial.available())
	{
		DeserializationError error = deserializeJson((*jsonDocument), Serial);
		// free(Serial);
		if (error)
		{
			Serial.print(F("deserializeJson() failed: "));
			Serial.println(error.f_str());
			return;
		}
		Serial.flush();

		String task_s = (*jsonDocument)["task"];
		char task[50];
		task_s.toCharArray(task, 256);

// jsonDocument.garbageCollect(); // memory leak?
/*if (task == "null") return;*/
#ifdef DEBUG_MAIN
		Serial.print("TASK: ");
		Serial.println(task);
#endif

		// do the processing based on the incoming stream
		if (strcmp(task, "multitable") == 0)
		{
			// multiple tasks
			tableProcessor(jsonDocument);
		}
		else
		{
			// Process individual tasks
			jsonProcessor(task, jsonDocument);
		}
	}
}

void SerialProcess::jsonProcessor(String task, DynamicJsonDocument *jsonDocument)
{
	/*
		Return state
	*/
	if (task == state_act_endpoint)
		state.act();
	if (task == state_set_endpoint)
		state.set();
	if (task == state_get_endpoint)
		state.get();
	/*
	  Drive Motors
	*/
	if (moduleController.get(AvailableModules::motor) != nullptr)
	{
		if (task == motor_act_endpoint)
		{
			moduleController.get(AvailableModules::motor)->act();
		}
		if (task == motor_set_endpoint)
		{
			moduleController.get(AvailableModules::motor)->set();
		}
		if (task == motor_get_endpoint)
		{
			moduleController.get(AvailableModules::motor)->get();
		}
	}

	/*
	  Home Motors
	*/
	if (moduleController.get(AvailableModules::home) != nullptr)
	{
		if (task == home_act_endpoint)
		{
			moduleController.get(AvailableModules::home)->act();
		}
		if (task == home_set_endpoint)
		{
			moduleController.get(AvailableModules::home)->set();
		}
		if (task == home_get_endpoint)
		{
			moduleController.get(AvailableModules::home)->get();
		}
	}


	/*
	  Operate SLM
	*/

	if (moduleController.get(AvailableModules::slm) != nullptr)
	{
		if (task == slm_act_endpoint)
		{
			moduleController.get(AvailableModules::slm)->act();
		}
		if (task == slm_set_endpoint)
		{
			moduleController.get(AvailableModules::slm)->set();
		}
		if (task == slm_get_endpoint)
		{
			moduleController.get(AvailableModules::slm)->get();
		}
	}
	/*
	  Drive DAC
	*/
	if (moduleController.get(AvailableModules::dac) != nullptr)
	{
		if (task == dac_act_endpoint)
			moduleController.get(AvailableModules::dac)->act();
		if (task == dac_set_endpoint)
			moduleController.get(AvailableModules::dac)->set();
		if (task == dac_get_endpoint)
			moduleController.get(AvailableModules::dac)->get();
	}
	/*
	  Drive Laser
	*/
	if (moduleController.get(AvailableModules::laser) != nullptr)
	{
		if (task == laser_act_endpoint)
			moduleController.get(AvailableModules::laser)->act();
		if (task == laser_set_endpoint)
			moduleController.get(AvailableModules::laser)->get();
		if (task == laser_get_endpoint)
			moduleController.get(AvailableModules::laser)->set();
	}
	/*
	  Drive analog
	*/
	if (moduleController.get(AvailableModules::analog) != nullptr)
	{
		if (task == analog_act_endpoint)
			moduleController.get(AvailableModules::analog)->act();
		if (task == analog_set_endpoint)
			moduleController.get(AvailableModules::analog)->set();
		if (task == analog_get_endpoint)
			moduleController.get(AvailableModules::analog)->get();
	}
	/*
	  Drive digital
	*/
	if (moduleController.get(AvailableModules::digital) != nullptr)
	{
		if (task == digital_act_endpoint)
			moduleController.get(AvailableModules::digital)->act();
		if (task == digital_set_endpoint)
			moduleController.get(AvailableModules::digital)->set();
		if (task == digital_get_endpoint)
			moduleController.get(AvailableModules::digital)->get();
	}
	/*
	  Drive LED Matrix
	*/
	if (moduleController.get(AvailableModules::led) != nullptr)
	{
		if (task == ledarr_act_endpoint)
			moduleController.get(AvailableModules::led)->act();
		if (task == ledarr_set_endpoint)
			moduleController.get(AvailableModules::led)->set();
		if (task == ledarr_get_endpoint)
			moduleController.get(AvailableModules::led)->get();
	}

	/*
	  Read the sensor
	*/
	if (moduleController.get(AvailableModules::sensor) != nullptr)
	{
		if (task == readsensor_act_endpoint)
			moduleController.get(AvailableModules::sensor)->act();
		if (task == readsensor_set_endpoint)
			moduleController.get(AvailableModules::sensor)->set();
		if (task == readsensor_get_endpoint)
			moduleController.get(AvailableModules::sensor)->get();
	}

	/*
	  Control PID controller
	*/
	if (moduleController.get(AvailableModules::pid) != nullptr)
	{
		if (task == PID_act_endpoint)
			moduleController.get(AvailableModules::pid)->act();
		if (task == PID_set_endpoint)
			moduleController.get(AvailableModules::pid)->set();
		if (task == PID_get_endpoint)
			moduleController.get(AvailableModules::pid)->get();
	}

	if (task == scanwifi_endpoint)
	{
		RestApi::scanWifi();
	}
	if (task == connectwifi_endpoint)
	{
		RestApi::connectToWifi();
	}
	if (task == reset_nv_flash_endpoint)
	{
		RestApi::resetNvFLash();
	}

	if(task == modules_set_endpoint)
		moduleController.set();
	if(task == modules_get_endpoint)
		moduleController.get();

	// Send JSON information back
	Serial.println("++");
	serializeJson((*jsonDocument), Serial);
	Serial.println();
	Serial.println("--");
	jsonDocument->clear();
	jsonDocument->garbageCollect();
}

void SerialProcess::tableProcessor(DynamicJsonDocument *jsonDocument)
{
	// 1. Copy the table
	DynamicJsonDocument tmpJsonDoc = (*jsonDocument);
	jsonDocument->clear();

	// 2. now we need to extract the indidvidual tasks
	int N_tasks = tmpJsonDoc["task_n"];
	int N_repeats = tmpJsonDoc["repeats_n"];

	Serial.println("N_tasks");
	Serial.println(N_tasks);
	Serial.println("N_repeats");
	Serial.println(N_repeats);

	for (int irepeats = 0; irepeats < N_repeats; irepeats++)
	{
		for (int itask = 0; itask < N_tasks; itask++)
		{
			char json_string[256];
			// Hacky, but should work
			Serial.println(itask);
			serializeJson(tmpJsonDoc[String(itask)], json_string);
			Serial.println(json_string);
			deserializeJson((*jsonDocument), json_string);

			String task_s = (*jsonDocument)["task"];
			char task[50];
			task_s.toCharArray(task, 256);

// jsonDocument.garbageCollect(); // memory leak?
/*if (task == "null") return;*/
#ifdef DEBUG_MAIN
			Serial.print("TASK: ");
			Serial.println(task);
#endif
			jsonProcessor(task, jsonDocument);
		}
	}
	tmpJsonDoc.clear();
}
SerialProcess serial;