
#include "SerialProcess.h"
#include "../wifi/Endpoints.h"
#include "Arduino.h"
#include <PinConfig.h>
#include "../wifi/RestApiCallbacks.h"
#ifdef ANALOG_IN_CONTROLLER
#include "../analogin/AnalogInController.h"
#endif
#ifdef ANALOG_OUT_CONTROLLER
#include "../analogout/AnalogOutController.h"
#endif
#ifdef BLUETOOTH
#include "../bt/BtController.h"
#endif
#include "../config/ConfigController.h"
#ifdef DAC_CONTROLLER
#include "../dac/DacController.h"
#endif
#ifdef DIGITAL_IN_CONTROLLER
#include "../digitalin/DigitalInController.h"
#endif
#ifdef DIGITAL_OUT_CONTROLLER
#include "../digitalout/DigitalOutController.h"
#endif
#ifdef ENCODER_CONTROLLER
#include "../encoder/EncoderController.h"
#endif
#ifdef LINEAR_ENCODER_CONTROLLER
#include "../encoder/LinearEncoderController.h"
#endif
#ifdef HOME_MOTOR
#include "../home/HomeMotor.h"
#endif
#ifdef USE_I2C
#include "../i2c/i2c_controller.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif
#ifdef MESSAGE_CONTROLLER
#include "../message/MessageController.h"
#endif
#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#endif
#ifdef PID_CONTROLLER
#include "../pid/PidController.h"
#endif
#ifdef SCANNER_CONTROLLER
#include "../scanner/ScannerController.h"
#endif
#include "../state/State.h"
#ifdef WIFI
#include "../wifi/WifiController.h"
#endif

#ifdef HEAT_CONTROLLER
#include "../heat/HeatController.h"
#include "../heat/DS18b20Controller.h"
#endif

namespace SerialProcess
{
	QueueHandle_t serialMSGQueue;
	xTaskHandle xHandle;

	void serialTask(void *p)
	{
		cJSON root;
		for (;;)
		{
			xQueueReceive(serialMSGQueue, &root, portMAX_DELAY);
			cJSON *tasks = cJSON_GetObjectItemCaseSensitive(&root, "tasks");
			if (tasks != NULL)
			{

				// {"tasks":[{"task":"/state_get"},{"task":"/state_act", "delay":1000}],"nTimes":2}
				int nTimes = 1;
				// perform the table n-times
				if (cJSON_GetObjectItemCaseSensitive(&root, "nTimes")->valueint != NULL)
					nTimes = cJSON_GetObjectItemCaseSensitive(&root, "nTimes")->valueint;

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

				cJSON *string = cJSON_GetObjectItemCaseSensitive(&root, "task");
				char *ss = cJSON_GetStringValue(string);
				// log_i("Process task:%s", ss);
				jsonProcessor(ss, &root);
			}
		}
		cJSON_Delete(&root);
		vTaskDelete(NULL);
	}

	void setup()
	{
		if (serialMSGQueue == nullptr)
			serialMSGQueue = xQueueCreate(2, sizeof(cJSON));
		if (xHandle == nullptr)
			xTaskCreate(serialTask, "sendsocketmsg", pinConfig.BT_CONTROLLER_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, &xHandle);
	}

	void loop()
	{
		if (Serial.available())
		{
			String c = Serial.readString();
			const char *s = c.c_str();
			Serial.flush();
			log_i("String s:%s , char:%s", c.c_str(), s);
			cJSON *root = cJSON_Parse(s);
			if (root != NULL)
			{
				xQueueSend(serialMSGQueue, (void *)root, 0);
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

	void serialize(cJSON *doc)
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

	void serialize(int qid)
	{
		cJSON *doc = cJSON_CreateObject();
		cJSON *v = cJSON_CreateNumber(qid);
		cJSON_AddItemToObject(doc, "qid", v);
		cJSON_AddItemToObject(doc, "idsuccess", cJSON_CreateNumber(1));
		Serial.println("++");
		char *s = cJSON_Print(doc);
		Serial.println(s);
		cJSON_Delete(doc);
		free(s);
		Serial.println();
		Serial.println("--");
	}

	void jsonProcessor(char *task, cJSON *jsonDocument)
	{

#ifdef ANALOG_OUT_CONTROLLER
		if (strcmp(task, analogout_act_endpoint) == 0)
			serialize(AnalogOutController::act(jsonDocument));
		if (strcmp(task, analogout_get_endpoint) == 0)
			serialize(AnalogOutController::get(jsonDocument));
#endif

#ifdef BLUETOOTH
		if (strcmp(task, bt_connect_endpoint) == 0)
		{
			// {"task":"/bt_connect", "mac":"1a:2b:3c:01:01:01", "psx":2}
			char *mac = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(jsonDocument, "mac")); // jsonDocument["mac"];
			int ps = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(jsonDocument, "psx"));	 // jsonDocument["psx"];
#ifdef BTHID
			BtController::setMacAndConnect(mac);
#endif
#ifdef PSXCONTROLLER
			BtController::connectPsxController(mac, ps);
#endif
		}
		if (strcmp(task, bt_scan_endpoint) == 0)
		{
			BtController::scanForDevices(jsonDocument);
		}
#endif

#ifdef DAC_CONTROLLER
		if (strcmp(task, dac_act_endpoint) == 0)
			serialize(DacController::act(jsonDocument));
			// if (strcmp(task, dac_get_endpoint) == 0)
			//	serialize(DacController::get(jsonDocument));
#endif

#ifdef DIGITAL_IN_CONTROLLER
		if (strcmp(task, digitalin_act_endpoint) == 0)
			serialize(DigitalInController::act(jsonDocument));
		if (strcmp(task, digitalin_get_endpoint) == 0)
			serialize(DigitalInController::get(jsonDocument));
#endif
#ifdef DIGITAL_OUT_CONTROLLER
		if (strcmp(task, digitalout_act_endpoint) == 0)
			serialize(DigitalOutController::act(jsonDocument));
		if (strcmp(task, digitalout_get_endpoint) == 0)
			serialize(DigitalOutController::get(jsonDocument));
#endif

#ifdef ENCODER_CONTROLLER
		if (strcmp(task, encoder_act_endpoint) == 0)
			serialize(EncoderController::act(jsonDocument));
		if (strcmp(task, encoder_get_endpoint) == 0)
			serialize(EncoderController::get(jsonDocument));
#endif

/*
	  LinearEncoders
	*/
#ifdef LINEAR_ENCODER_CONTROLLER
		if (strcmp(task, linearencoder_act_endpoint) == 0)
		{
			serialize(LinearEncoderController::act(jsonDocument));
		}
		if (strcmp(task, linearencoder_get_endpoint) == 0)
		{
			serialize(LinearEncoderController::get(jsonDocument));
		}
#endif
#ifdef HOME_MOTOR
	if (strcmp(task, home_get_endpoint) == 0)
		serialize(HomeMotor::get(jsonDocument));
	if (strcmp(task, home_act_endpoint) == 0)
		serialize(HomeMotor::act(jsonDocument));
#endif
#ifdef USE_I2C
	if (strcmp(task, i2c_get_endpoint) == 0)
		serialize(i2c_controller::get(jsonDocument));
	if (strcmp(task, i2c_act_endpoint) == 0)
		serialize(i2c_controller::act(jsonDocument));
#endif
#ifdef LASER_CONTROLLER
		if (strcmp(task, laser_get_endpoint) == 0)
			serialize(LaserController::get(jsonDocument));
		if (strcmp(task, laser_act_endpoint) == 0)
			serialize(LaserController::act(jsonDocument));
#endif
#ifdef LED_CONTROLLER
		if (strcmp(task, ledarr_get_endpoint) == 0)
			serialize(LedController::get(jsonDocument));
		if (strcmp(task, ledarr_act_endpoint) == 0)
			serialize(LedController::act(jsonDocument));
#endif
#ifdef MESSAGE_CONTROLLER
		if (strcmp(task, message_get_endpoint) == 0)
			serialize(MessageController::get(jsonDocument));
		if (strcmp(task, message_act_endpoint) == 0)
			serialize(MessageController::act(jsonDocument));
#endif
#ifdef MOTOR_CONTROLLER
		if (strcmp(task, motor_get_endpoint) == 0)
			serialize(FocusMotor::get(jsonDocument));
		if (strcmp(task, motor_act_endpoint) == 0)
			serialize(FocusMotor::act(jsonDocument));
#endif
#ifdef PID_CONTROLLER
		if (strcmp(task, PID_get_endpoint) == 0)
			serialize(PidController::get(jsonDocument));
		if (strcmp(task, PID_act_endpoint) == 0)
			serialize(PidController::act(jsonDocument));
#endif

		if (strcmp(task, state_act_endpoint) == 0)
			serialize(State::act(jsonDocument));
		if (strcmp(task, state_get_endpoint) == 0)
			serialize(State::get(jsonDocument));

		if (strcmp(task, modules_get_endpoint) == 0)
		{
			serialize(State::getModules());
		}
#ifdef WIFI
		if (strcmp(task, scanwifi_endpoint) == 0)
		{
			serialize(WifiController::scan());
		}
		// {"task":"/wifi/scan"}
		if (strcmp(task, connectwifi_endpoint) == 0)
		{ // {"task":"/wifi/connect","ssid":"Test","PW":"12345678", "AP":false}
			WifiController::connect(jsonDocument);
		}
#endif
#ifdef HEAT_CONTROLLER
		if (strcmp(task, heat_get_endpoint) == 0)
			serialize(HeatController::get(jsonDocument));
		if (strcmp(task, heat_act_endpoint) == 0)
			serialize(HeatController::act(jsonDocument));
		if (strcmp(task, ds18b20_get_endpoint) == 0)
			serialize(DS18b20Controller::get(jsonDocument));
		if (strcmp(task, ds18b20_act_endpoint) == 0)
			serialize(DS18b20Controller::act(jsonDocument));
#endif
	}
}
