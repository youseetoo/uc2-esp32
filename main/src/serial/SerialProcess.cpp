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
	// Queue-based processing to handle serial load without blocking main thread
	QueueHandle_t serialMSGQueue = nullptr; // Queue that buffers incoming messages and delegates them to the appropriate task
	xTaskHandle xHandle = nullptr;		  // Task handle for the serial task
	
	// Serial output queue to prevent race conditions
	QueueHandle_t serialOutputQueue = nullptr; // Queue for serial output strings
	xTaskHandle xOutputHandle = nullptr;       // Task handle for serial output task
	
	// Structure to hold serial output messages
	struct SerialMessage {
		char* message;
		size_t length;
	};

	// Critical section mutex to prevent serial output interruption
	portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

	// Serial output task - processes queued output to prevent race conditions
	void serialOutputTask(void *p)
	{
		for (;;)
		{
			SerialMessage msg;
			if (xQueueReceive(serialOutputQueue, &msg, portMAX_DELAY) == pdTRUE)
			{
				if (msg.message != nullptr)
				{
					// Direct serial write without critical section - this task has exclusive access
					Serial.write((uint8_t*)msg.message, msg.length);
					Serial.flush();
					
					// Free the allocated message
					free(msg.message);
				}
			}
		}
		vTaskDelete(NULL);
	}

	// Thread-safe Serial.println replacement
	void safePrintln(const char* message)
	{
		if (message == nullptr || serialOutputQueue == nullptr)
			return;

		size_t len = strlen(message);
		if (len == 0 || len > 8192)
			return;

		// Allocate buffer for message + newline
		char* buffer = (char*)malloc(len + 2);
		if (buffer == nullptr)
		{
			log_e("Failed to allocate serial output buffer");
			return;
		}

		// Copy message and add newline
		memcpy(buffer, message, len);
		buffer[len] = '\n';
		buffer[len + 1] = '\0';

		SerialMessage msg;
		msg.message = buffer;
		msg.length = len + 1;

		// Try to send to queue with timeout
		if (xQueueSend(serialOutputQueue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
		{
			// Queue full - free buffer and drop message
			free(buffer);
			log_w("Serial output queue full - dropping message");
		}
	}

	void serialTask(void *p)
	{
		for (;;)
		{
			cJSON *root = NULL;
			xQueueReceive(serialMSGQueue, &root, portMAX_DELAY);
			if (root == NULL)
				continue; // Handle NULL case

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
		vTaskDelete(NULL);
	}

	// Thread-safe JSON serial output function
	void safeSerializeJson(cJSON *doc)
	{
		if (doc == NULL)
		{
			safePrintln("{\"error\":\"NULL JSON document\"}");
			return;
		}

		// CRITICAL: Serialize JSON to string immediately
		// The caller must ensure the cJSON object remains valid during this call
		char *s = cJSON_PrintUnformatted(doc);
		if (s == NULL)
		{
			safePrintln("{\"error\":\"Failed to serialize JSON\"}");
			return;
		}

		// Send the serialized string and free it
		safeSendJsonString(s);
		free(s);
	}

	// Send a pre-serialized JSON string with protocol delimiters
	// This is useful when JSON serialization needs to happen in a protected context
	// The caller is responsible for freeing jsonString after calling this function
	void safeSendJsonString(char* jsonString)
	{
		if (jsonString == NULL)
		{
			safePrintln("{\"error\":\"NULL JSON string\"}");
			return;
		}

		// Build complete message with delimiters
		size_t len = strlen(jsonString);
		if (len == 0 || len > 8192)
		{
			safePrintln("{\"error\":\"JSON string too large or empty\"}");
			return;
		}

		// Calculate buffer size: "++\n" (3) + jsonString (len) + "\n--\n" (4) + null terminator (1)
		size_t totalLen = len + 8; // 3 + len + 4 + 1 for null terminator
		char* buffer = (char*)malloc(totalLen);
		if (buffer == nullptr)
		{
			safePrintln("{\"error\":\"Out of memory\"}");
			return;
		}

		// Construct message: "++\n<json>\n--\n"
		strcpy(buffer, "++\n");
		strcat(buffer, jsonString);
		strcat(buffer, "\n--\n");

		// Send through output queue
		if (serialOutputQueue != nullptr)
		{
			SerialMessage msg;
			msg.message = buffer;
			msg.length = strlen(buffer) + 1; // Include newline

			if (xQueueSend(serialOutputQueue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
			{
				free(buffer);
				log_w("Serial output queue full - dropping JSON string");
			}
		}
		else
		{
			free(buffer);
		}
	}

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
		log_i("SerialProcess::setup() starting");
		
		// Create queue and separate task for serial processing
		if (serialMSGQueue == nullptr)
			serialMSGQueue = xQueueCreate(5, sizeof(cJSON *)); // Queue for cJSON pointers (increased size)
		if (xHandle == nullptr)
			xTaskCreate(serialTask, "serialtask", pinConfig.BT_CONTROLLER_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, &xHandle);
		
		// Create queue and task for serial output to prevent race conditions
		if (serialOutputQueue == nullptr)
			serialOutputQueue = xQueueCreate(10, sizeof(SerialMessage)); // Queue for serial output
		if (xOutputHandle == nullptr)
			xTaskCreate(serialOutputTask, "serialout", 3072, NULL, pinConfig.DEFAULT_TASK_PRIORITY + 1, &xOutputHandle); // Higher priority
		
		Serial.setTimeout(100);
		Serial.setTxBufferSize(1024);
		
		log_i("SerialProcess::setup() completed - Ready to receive serial data");
		Serial.println("DEBUG: SerialProcess ready");
		//esp_log_level_set("*", ESP_LOG_NONE); // FIXME: This causes the counter to fail - and in general the ESP32s3 serial output too
		//Serial.setDebugOutput(false);
	}

	void addJsonToQueue(cJSON *doc)
	{
		// This bypasses the serial input and directly adds a cJSON object to the queue (e.g. via I2C)
		if (serialMSGQueue != nullptr) {
			if (xQueueSend(serialMSGQueue, &doc, 0) != pdTRUE) {
				// Queue full - delete doc to prevent memory leak
				cJSON_Delete(doc);
				log_w("Serial queue full - dropping message");
			}
		}
	}

	void loop()
	{
		// Try to read and queue serial data if available
		if (Serial.available()) {
			int bytesAvailable = Serial.available();
			//log_i("Serial RX: %d bytes available", bytesAvailable);
			
			String command = Serial.readString();  // Keep String alive during parsing
			command.trim(); // Remove any whitespace/newlines
			
			//log_i("Serial RX: Read %d chars: %s", command.length(), command.c_str());
			
			if (command.length() > 0) {
				cJSON *doc = cJSON_Parse(command.c_str());
				if (doc) {
					//log_i("Serial RX: JSON parsed successfully");
					addJsonToQueue(doc);
				} else {
					// send {"error":"Failed to parse JSON"} back
					cJSON *errorResponse = cJSON_CreateObject();
					if (errorResponse != NULL) {
						cJSON_AddStringToObject(errorResponse, "error", "Failed to parse JSON");
						serialize(errorResponse);
					}
					// log_w("Failed to parse serial JSON: %s", command.c_str());
				}
			} 
		}

		// Let other tasks run
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	void serialize(cJSON *doc)
	{
		// Use the new thread-safe function
		safeSerializeJson(doc);
		// IMPORTANT: Only delete the doc if it's a new object created by a controller
		// The serialTask is responsible for deleting the root request object
		// This function is called with RESPONSE objects created by controllers (get/act functions)
		if (doc != NULL) {
			cJSON_Delete(doc);
		}
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

		// Print the JSON document to a string
		char *s = cJSON_PrintUnformatted(doc);
		cJSON_Delete(doc); // Free the cJSON object
		
		if (s == NULL)
			return;

		// Build complete message with delimiters
		size_t len = strlen(s);
		// Calculate buffer size: "++\n" (3) + s (len) + "\n--\n" (4) + null terminator (1)
		size_t totalLen = len + 8; // 3 + len + 4 + 1 for null terminator
		char* buffer = (char*)malloc(totalLen);
		if (buffer == nullptr)
		{
			free(s);
			return;
		}

		// Construct message: "++\n<json>\n--\n"
		strcpy(buffer, "++\n");
		strcat(buffer, s);
		strcat(buffer, "\n--\n");
		free(s);

		// Send through output queue
		if (serialOutputQueue != nullptr)
		{
			SerialMessage msg;
			msg.message = buffer;
			msg.length = strlen(buffer) + 1; // Include newline
			
			if (xQueueSend(serialOutputQueue, &msg, pdMS_TO_TICKS(100)) != pdTRUE)
			{
				free(buffer);
				log_w("Serial output queue full - dropping qid message");
			}
		}
		else
		{
			free(buffer);
		}
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
		else if (strcmp(task, bt_disconnect_endpoint) == 0)
		{
			BtController::disconnect();
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
			// Build message and send through queue
			char buffer[32];
			snprintf(buffer, sizeof(buffer), "++{'b':%d}--", State::getBusy());
			safePrintln(buffer);
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
