#include <PinConfig.h>
#include "SerialProcess.h"
#include "../wifi/Endpoints.h"
#include "Arduino.h"
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
#ifdef OBJECTIVE_CONTROLLER
#include "../objective/ObjectiveController.h"
#endif
#ifdef I2C_MASTER
#include "../i2c/i2c_master.h"
#endif
#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
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
#include "../motor/MotorJsonParser.h"
#endif
#ifdef PID_CONTROLLER
#include "../pid/PidController.h"
#endif
#ifdef SCANNER_CONTROLLER
#include "../scanner/ScannerController.h"
#endif
#ifdef GALVO_CONTROLLER
#include "../scanner/GalvoController.h"
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
	// Removed queue-based processing to fix ESP32S3 serial corruption
	// Process JSON commands immediately in main loop instead of separate task
	// QueueHandle_t serialMSGQueue; // Queue that buffers incoming messages and delegates them to the appropriate task
	// xTaskHandle xHandle;		  // Task handle for the serial task

	// Critical section mutex to prevent serial output interruption
	portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

	void processJsonDocument(cJSON *root)
	{
		if (root == NULL)
			return; // Handle NULL case

		cJSON *tasks = cJSON_GetObjectItemCaseSensitive(root, "tasks");
		if (tasks != NULL)
		{
			int nTimes = 1;
			cJSON *nTimesItem = cJSON_GetObjectItemCaseSensitive(root, "nTimes");
			if (nTimesItem != NULL)
				nTimes = nTimesItem->valueint;

			for (int i = 0; i < nTimes; i++)
			{
				log_i("Process tasks");
				cJSON *t = NULL;
				cJSON_ArrayForEach(t, tasks)
				{
					cJSON *ta = cJSON_GetObjectItemCaseSensitive(t, "task");
					if (ta != NULL)
					{
						char *string = cJSON_GetStringValue(ta);
						if (string != NULL)
							jsonProcessor(string, t);
					}
				}
			}
		}
		else
		{
			cJSON *string = cJSON_GetObjectItemCaseSensitive(root, "task");
			if (string != NULL)
			{
				char *ss = cJSON_GetStringValue(string);
				if (ss != NULL)
					jsonProcessor(ss, root);
			}
		}

		cJSON_Delete(root); // Delete root after processing
	}

	void setup()
	{
		// No longer creating queue or separate task - processing happens in main loop
		// if (serialMSGQueue == nullptr)
		//	serialMSGQueue = xQueueCreate(2, sizeof(cJSON *)); // Queue for cJSON pointers
		// if (xHandle == nullptr)
		//	xTaskCreate(serialTask, "sendsocketmsg", pinConfig.BT_CONTROLLER_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, &xHandle);
		Serial.setTimeout(100);
		Serial.setTxBufferSize(1024);
		//esp_log_level_set("*", ESP_LOG_NONE); // FIXME: This causes the counter to fail - and in general the ESP32s3 serial output too
		//Serial.setDebugOutput(false);
	}

	void addJsonToQueue(cJSON *doc)
	{
		// This bypasses the serial input and directly processes cJSON object (e.g. via I2C)
		// No longer using queue - process immediately to prevent serial corruption
		processJsonDocument(doc);
	}

	void loop()
	{
		if (Serial.available())
		{
			String c = Serial.readString();
			const char *s = c.c_str();
			Serial.flush();
			// log_i("String s:%s , char:%s", c.c_str(), s);
			cJSON *root = cJSON_Parse(s);
			if (root != NULL)
			{
				// Process immediately instead of adding to queue to prevent serial corruption
				processJsonDocument(root);
			}
			else
			{
				const char *error_ptr = cJSON_GetErrorPtr();
				if (error_ptr != NULL)
					log_i("Error while parsing:%s", error_ptr);
				log_i("Serial input is null");
				// Use critical section for error output to prevent interruption
				portENTER_CRITICAL_ISR(&mux);
				Serial.println("++{\"error\":\"Serial input is null\"}--");
				Serial.flush();
				portEXIT_CRITICAL_ISR(&mux);
			}
			c.clear();
		}
	}

	void serialize(cJSON *doc)
	{
		// e.g. used for state_get or sendMotorPos()
		if (doc == NULL)
		{
			Serial.println("{\"error\":\"NULL JSON document\"}");
			return;
		}

		// Print the JSON document to a string
		char *s = cJSON_PrintUnformatted(doc);
		if (s == NULL)
		{
			cJSON_Delete(doc);
			Serial.println("{\"error\":\"Failed to serialize JSON\"}");
			return;
		}

		// Check string length to prevent buffer overflow
		size_t len = strlen(s);
		if (len == 0 || len > 8192) // Reasonable limit for serial output
		{
			free(s);
			cJSON_Delete(doc);
			Serial.println("{\"error\":\"Response too large or empty\"}");
			return;
		}

		// Use critical section only for the actual serial output, not JSON processing
		portENTER_CRITICAL_ISR(&mux);
		Serial.println("++");
		
		// Try to write in chunks if too large to prevent UART buffer overflow
		if (len > 1024)
		{
			// Exit critical section temporarily for large outputs
			portEXIT_CRITICAL_ISR(&mux);
			
			// Write in 512 byte chunks to prevent UART buffer overflow
			for (size_t i = 0; i < len; i += 512)
			{
				size_t chunk_size = (len - i > 512) ? 512 : (len - i);
				Serial.write((uint8_t*)(s + i), chunk_size);
				vTaskDelay(pdMS_TO_TICKS(1)); // Small delay between chunks
			}
			Serial.println(); // Add final newline
			
			// Re-enter critical section for closing
			portENTER_CRITICAL_ISR(&mux);
		}
		else
		{
			Serial.println(s);
		}
		
		Serial.println("--");
		Serial.flush();
		portEXIT_CRITICAL_ISR(&mux);

		free(s); // Free the string created by cJSON_Print
		cJSON_Delete(doc); // Free the cJSON object
	}

	void serialize(int qid)
	{
		// used for most of the returns of act/get functions
		cJSON *doc = cJSON_CreateObject();
		if (doc == NULL)
			return; // Handle memory allocation failure

		cJSON *v = cJSON_CreateNumber(qid);
		if (v != NULL)
		{
			cJSON_AddItemToObject(doc, "qid", v);
		}
		// indicate success or failure based on qid value
		if (qid > 0)
			cJSON_AddItemToObject(doc, "success", cJSON_CreateNumber(1));
		else if (qid < 0)
			cJSON_AddItemToObject(doc, "success", cJSON_CreateNumber(-1));
		else
			cJSON_AddItemToObject(doc, "success", cJSON_CreateNumber(0));

		// Use critical section to prevent serial output interruption on ESP32S3
		portENTER_CRITICAL_ISR(&mux);
		Serial.println("++");

		// Print the JSON document to a string
		char *s = cJSON_PrintUnformatted(doc);
		if (s != NULL)
		{
			Serial.println(s);
			free(s); // Free the string created by cJSON_Print
		}

		cJSON_Delete(doc); // Free the cJSON object

		// Serial.println();
		Serial.println("--");
		Serial.flush();
		portEXIT_CRITICAL_ISR(&mux);
	}

	void jsonProcessor(char *task, cJSON *jsonDocument)
	{

		/*
		This function takes in the task (e.g. /state_get)
		and a JSON object that holds the information for the
		task that needs to be processed. It calls the get/act
		function for the different controllers*/
		if (false) // keep all other else ifs happy
			return;
#ifdef ANALOG_OUT_CONTROLLER
		else if (strcmp(task, analogout_act_endpoint) == 0)
			serialize(AnalogOutController::act(jsonDocument));
		else if (strcmp(task, analogout_get_endpoint) == 0)
			serialize(AnalogOutController::get(jsonDocument));
#endif

#ifdef BLUETOOTH
		else if (strcmp(task, bt_connect_endpoint) == 0)
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
		else if (strcmp(task, bt_scan_endpoint) == 0)
		{
			BtController::scanForDevices(jsonDocument);
		}
#endif

#ifdef DAC_CONTROLLER
		else if (strcmp(task, dac_act_endpoint) == 0)
			serialize(DacController::act(jsonDocument));
		// else  if (strcmp(task, dac_get_endpoint) == 0)
		//	serialize(DacController::get(jsonDocument));
#endif

#ifdef DIGITAL_IN_CONTROLLER
		else if (strcmp(task, digitalin_act_endpoint) == 0)
			serialize(DigitalInController::act(jsonDocument));
		else if (strcmp(task, digitalin_get_endpoint) == 0)
			serialize(DigitalInController::get(jsonDocument));
#endif
#ifdef DIGITAL_OUT_CONTROLLER
		else if (strcmp(task, digitalout_act_endpoint) == 0)
			serialize(DigitalOutController::act(jsonDocument));
		else if (strcmp(task, digitalout_get_endpoint) == 0)
			serialize(DigitalOutController::get(jsonDocument));
#endif

#ifdef ENCODER_CONTROLLER
		else if (strcmp(task, encoder_act_endpoint) == 0)
			serialize(EncoderController::act(jsonDocument));
		else if (strcmp(task, encoder_get_endpoint) == 0)
			serialize(EncoderController::get(jsonDocument));
#endif

/*
	  LinearEncoders
	*/
#ifdef LINEAR_ENCODER_CONTROLLER
		else if (strcmp(task, linearencoder_act_endpoint) == 0)
		{
			serialize(LinearEncoderController::act(jsonDocument));
		}
		else if (strcmp(task, linearencoder_get_endpoint) == 0)
		{
			serialize(LinearEncoderController::get(jsonDocument));
		}
#endif
#ifdef HOME_MOTOR
		else if (strcmp(task, home_get_endpoint) == 0)
			serialize(HomeMotor::get(jsonDocument));
		else if (strcmp(task, home_act_endpoint) == 0)
			serialize(HomeMotor::act(jsonDocument));
#endif
#ifdef OBJECTIVE_CONTROLLER
		else if (strcmp(task, objective_get_endpoint) == 0)
			serialize(ObjectiveController::get(jsonDocument));
		else if (strcmp(task, objective_act_endpoint) == 0)
			serialize(ObjectiveController::act(jsonDocument));
#endif
#ifdef I2C_MASTER
		else if (strcmp(task, i2c_get_endpoint) == 0)
			serialize(i2c_master::get(jsonDocument));
		else if (strcmp(task, i2c_act_endpoint) == 0)
			serialize(i2c_master::act(jsonDocument));
#endif
#ifdef CAN_CONTROLLER
		else if (strcmp(task, can_get_endpoint) == 0)
			serialize(can_controller::get(jsonDocument));
		else if (strcmp(task, can_act_endpoint) == 0)
			serialize(can_controller::act(jsonDocument));
#endif
#ifdef LASER_CONTROLLER
		else if (strcmp(task, laser_get_endpoint) == 0)
			serialize(LaserController::get(jsonDocument));
		else if (strcmp(task, laser_act_endpoint) == 0)
			serialize(LaserController::act(jsonDocument));
#endif
#ifdef TMC_CONTROLLER
		else if (strcmp(task, tmc_get_endpoint) == 0)
			serialize(TMCController::get(jsonDocument));
		else if (strcmp(task, tmc_act_endpoint) == 0)
			serialize(TMCController::act(jsonDocument));
#endif
#ifdef LED_CONTROLLER
		else if (strcmp(task, ledarr_get_endpoint) == 0)
			serialize(LedController::get(jsonDocument));
		else if (strcmp(task, ledarr_act_endpoint) == 0)
			serialize(LedController::act(jsonDocument));
#endif
#ifdef MESSAGE_CONTROLLER
		else if (strcmp(task, message_get_endpoint) == 0)
			serialize(MessageController::get(jsonDocument));
		else if (strcmp(task, message_act_endpoint) == 0)
			serialize(MessageController::act(jsonDocument));
#endif
#ifdef MOTOR_CONTROLLER
		else if (strcmp(task, motor_get_endpoint) == 0)
			serialize(MotorJsonParser::get(jsonDocument));
		else if (strcmp(task, motor_act_endpoint) == 0)
			serialize(MotorJsonParser::act(jsonDocument));
#endif
#ifdef SCANNER_CONTROLLER
		else if (strcmp(task, scanner_get_endpoint) == 0)
			serialize(ScannerController::get(jsonDocument));
		else if (strcmp(task, scanner_act_endpoint) == 0)
			serialize(ScannerController::act(jsonDocument));
#endif
#ifdef GALVO_CONTROLLER
		else if (strcmp(task, galvo_get_endpoint) == 0)
			serialize(GalvoController::get(jsonDocument));
		else if (strcmp(task, galvo_act_endpoint) == 0)
			serialize(GalvoController::act(jsonDocument));
#endif
#ifdef PID_CONTROLLER
		else if (strcmp(task, PID_get_endpoint) == 0)
			serialize(PidController::get(jsonDocument));
		else if (strcmp(task, PID_act_endpoint) == 0)
			serialize(PidController::act(jsonDocument));
#endif

		else if (strcmp(task, state_act_endpoint) == 0)
			serialize(State::act(jsonDocument));
		else if (strcmp(task, state_get_endpoint) == 0)
			serialize(State::get(jsonDocument));
		else if (strcmp(task, state_busy_endpoint) == 0)
		{
			// super fast check if the system is busy {"task":"/b"}
			// Use critical section to prevent serial output interruption
			portENTER_CRITICAL_ISR(&mux);
			Serial.print("++{'b':");
			Serial.print(State::getBusy());
			Serial.println("}--");
			portEXIT_CRITICAL_ISR(&mux);
		}
		else if (strcmp(task, modules_get_endpoint) == 0)
		{
			serialize(State::getModules());
		}
#ifdef WIFI
		else if (strcmp(task, scanwifi_endpoint) == 0)
		{
			serialize(WifiController::scan());
		}
		// {"task":"/wifi/scan"}
		else if (strcmp(task, connectwifi_endpoint) == 0)
		{ // {"task":"/wifi/connect","ssid":"Test","PW":"12345678", "AP":false}
			WifiController::connect(jsonDocument);
		}
#endif
#ifdef HEAT_CONTROLLER
		else if (strcmp(task, heat_get_endpoint) == 0)
			serialize(HeatController::get(jsonDocument));
		else if (strcmp(task, heat_act_endpoint) == 0)
			serialize(HeatController::act(jsonDocument));
		else if (strcmp(task, ds18b20_get_endpoint) == 0)
			serialize(DS18b20Controller::get(jsonDocument));
		else if (strcmp(task, ds18b20_act_endpoint) == 0)
			serialize(DS18b20Controller::act(jsonDocument));
#endif
		else
		{
			// Unknown task
			serialize(-1);
		}
	}
}
