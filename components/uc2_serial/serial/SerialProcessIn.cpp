#include <PinConfig.h>
#include "SerialProcess.h"
#include "Endpoints.h"
// T4.1: the serial OUTPUT side (safeSendJsonString/safePrintln/safeSerializeJson
// + the output queue + task) was extracted to the pure-C uc2_serial_out.c. This
// file keeps the INPUT side (serialTask/loop/jsonProcessor) + the serialize()
// wrappers (which route through the C output).
// T1.3: direct IDF (no Arduino) — uc2 façade for Serial / millis / delay /
// log_* / ESP.restart(); FreeRTOS types come along via uc2_time.h.
#include <cstdlib>  // malloc/free
#include <cstring>  // strlen/strcpy/strcat/memcpy
#include "uc2_hal/uc2_esp.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_serial.h"
#include "uc2_hal/uc2_time.h"
#include "uc2_serial_out.h"   // T4.1: pure-C serial output (safeSendJsonString/…)
#ifdef ANALOG_IN_CONTROLLER
#include "AnalogInController.h"
#endif
#ifdef ANALOG_OUT_CONTROLLER
#include "AnalogOutController.h"
#endif
#ifdef BLUETOOTH
#include "BtController.h"
#endif
#include "ConfigController.h"
#include "RuntimeConfig.h"
#include "NVSConfig.h"
#ifdef DAC_CONTROLLER
#include "DacController.h"
#endif
#ifdef DIGITAL_IN_CONTROLLER
#include "DigitalInController.h"
#endif
#ifdef DIGITAL_OUT_CONTROLLER
#include "DigitalOutController.h"
#endif
#ifdef HOME_MOTOR
#include "HomeMotor.h"
#endif
#ifdef I2C_MASTER
#include "i2c_master.h"
#endif
#ifdef TMC_CONTROLLER
#include "TMCController.h"
#endif
#ifdef CAN_CONTROLLER_CANOPEN
#include "CANopenModule.h"
#ifndef CANOPEN_OTA_NO_SENDER
#include "OtaBinaryReceive.h"
#endif
#endif
#include "DeviceRouter.h"
#include "RoutingTable.h"
#include "cJsonTool.h"

#ifdef LASER_CONTROLLER
#include "LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "LedController.h"
#include "SignalController.h"  // needs Adafruit_NeoPixel, only in LED builds
#endif
#include "BuzzerController.h"
#ifdef MESSAGE_CONTROLLER
#include "MessageController.h"
#endif
#ifdef MOTOR_CONTROLLER
#include "MotorJsonParser.h"
#endif
#include "QidRegistry.h"
#ifdef PID_CONTROLLER
#include "PidController.h"
#endif
#ifdef SCANNER_CONTROLLER
#include "ScannerController.h"
#endif
#ifdef GALVO_CONTROLLER
#include "GalvoController.h"
#endif
#include "State.h"
#ifdef HEAT_CONTROLLER
#include "HeatController.h"
#include "DS18b20Controller.h"
#endif
#ifdef TMP102_CONTROLLER
#include "Tmp102Controller.h"
#endif
#ifdef FAN_CONTROLLER
#include "FanController.h"
#endif
#ifdef THERMAL_CONTROLLER
#include "ThermalController.h"
#endif

// T1.3: tag for the uc2 log_* façade (ESP_LOG* forwarding, see uc2_log.h).
static const char *TAG = "UC2_SERIAL";

namespace SerialProcess
{
	// Queue-based processing to handle serial load without blocking main thread
	QueueHandle_t serialMSGQueue = nullptr; // Queue that buffers incoming messages and delegates them to the appropriate task
	TaskHandle_t xHandle = nullptr;			// Task handle for the serial task

	// T4.1: the output queue/task + safePrintln/safeSendJsonString/safeSerializeJson
	// now live in the pure-C uc2_serial_out.c (see uc2_serial_out.h). This file
	// keeps only the INPUT side (serialTask/loop) + the serialize() wrappers.

	void serialTask(void *p)
	{
		for (;;)
		{
			cJSON *root = NULL;
			xQueueReceive(serialMSGQueue, &root, portMAX_DELAY);
			if (root == NULL){
				log_w("Received NULL JSON document in serial task");
				continue; // Handle NULL case
			}

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
					log_i("Process single task: %s", ss);
					if (ss != NULL)
						jsonProcessor(ss, root);
				}
				else
				{
					log_w("No 'task' or 'tasks' field found in JSON document");
				}
			}

			cJSON_Delete(root); // Delete root after processing
		}
		vTaskDelete(NULL);
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
			else
			{
				log_w("No 'task' or 'tasks' field found in JSON document");
			}
		}

		cJSON_Delete(root); // Delete root after processing
	}

	void setup()
	{
		log_i("SerialProcess::setup() starting");

		// Create queue and separate task for serial processing
		if (serialMSGQueue == nullptr)
			serialMSGQueue = xQueueCreate(16, sizeof(cJSON *)); // Queue for cJSON pointers (increased size)
		if (xHandle == nullptr)
			xTaskCreate(serialTask, "serialtask", pinConfig.BT_CONTROLLER_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, &xHandle);

		// T4.1: initialize the pure-C serial output (queue + task). Pass the
		// board's output-task priority (was pinConfig.DEFAULT_TASK_PRIORITY + 1).
		serial_out_init((UBaseType_t)(pinConfig.DEFAULT_TASK_PRIORITY + 1));

		Serial.setTimeout(100);
		qid_setup();

		log_i("SerialProcess::setup() completed - Ready to receive serial data");
		Serial.println("DEBUG: SerialProcess ready");
		// esp_log_level_set("*", ESP_LOG_NONE); // FIXME: This causes the counter to fail - and in general the ESP32s3 serial output too
		// Serial.setDebugOutput(false);
	}

	void addJsonToQueue(cJSON *doc)
	{
		// This bypasses the serial input and directly adds a cJSON object to the queue (e.g. via I2C)
		if (serialMSGQueue != nullptr)
		{
			if (xQueueSend(serialMSGQueue, &doc, 0) != pdTRUE)
			{
				// Queue full - delete doc to prevent memory leak
				cJSON_Delete(doc);
				log_w("Serial queue full - dropping message");
			}
		}
		else
		{
			log_e("SerialMSGQueue not initialized - cannot add message");
			cJSON_Delete(doc);
		}
	}

	// Static buffer for serial reading to avoid heap fragmentation
	static char serialInputBuffer[8192]; // 8KB buffer for large JSON strings
	static size_t serialInputPos = 0;
	static bool inJsonObject = false;
	static int braceCount = 0;
	static bool inStringLiteral = false; // inside a "..." — braces there don't nest
	static bool escapeNext = false;		 // previous char was a backslash in a string
	static uint32_t frameLastByteMs = 0; // last time a byte of this frame arrived

	// A frame that stops arriving mid-object is dropped after this long. Covers
	// a sender that dies (or never terminates the line at all) — without it the
	// partial frame would sit in the buffer and swallow the next command.
	static const uint32_t SERIAL_FRAME_TIMEOUT_MS = 2000;

	static void resetSerialFrame()
	{
		inJsonObject = false;
		serialInputPos = 0;
		braceCount = 0;
		inStringLiteral = false;
		escapeNext = false;
	}

	// Answer a bad line with a JSON error rather than silently dropping it.
	static void reportSerialError(const char *reason)
	{
		cJSON *errorResponse = cJSON_CreateObject();
		if (errorResponse != NULL)
		{
			cJSON_AddStringToObject(errorResponse, "error", reason);
			serialize(errorResponse); // serialize() takes ownership
		}
	}

	void loop()
	{
#if defined(CAN_CONTROLLER_CANOPEN) && !defined(CANOPEN_OTA_NO_SENDER)
		// CANopen path: /ota_start preamble flips this on. Raw firmware bytes
		// are streamed in 4 KB chunks straight to the slave via SDO — no
		// full-image buffer is held on the master.
		// TODO: We are limited to 1khz due to the loop task delay, or?
		if (OtaBinaryReceive::isActive())
		{
			OtaBinaryReceive::processBytes();
			vTaskDelay(pdMS_TO_TICKS(1));
			return;
		}
#endif

		// Read serial data byte-by-byte to handle large JSON strings reliably.
		// Framing rule: one line = one command. A frame starts at '{' and ends
		// at the matching '}'; a newline before that means the line was
		// malformed, and it is dropped so the *next* command still parses.
		while (Serial.available() > 0)
		{
			char c = Serial.read();

			// End of line — either we already completed the object (nothing to
			// do) or the object never closed and this line is garbage. Without
			// this, a missing '}' left braceCount stuck above zero and every
			// following command was appended to the same dead frame, which is
			// what wedged the interface until the 8 KB buffer overflowed.
			if (c == '\n' || c == '\r')
			{
				if (inJsonObject)
				{
					log_w("Discarding malformed JSON line (%u bytes, %d brace(s) unclosed)",
						  (unsigned)serialInputPos, braceCount);
					resetSerialFrame();
					reportSerialError("Malformed JSON - unterminated object");
				}
				continue;
			}

			// Outside a frame, ignore noise until an object starts.
			if (!inJsonObject)
			{
				if (c != '{')
					continue;
				resetSerialFrame();
				inJsonObject = true;
			}

			frameLastByteMs = millis();

			// Prevent buffer overflow
			if (serialInputPos >= sizeof(serialInputBuffer) - 1)
			{
				log_e("Serial input buffer overflow");
				resetSerialFrame();
				reportSerialError("Input buffer overflow");
				continue;
			}
			serialInputBuffer[serialInputPos++] = c;

			// Brace tracking, string-aware: a '{' or '}' inside a string literal
			// (e.g. {"task":"/x","name":"a{b"}) must not change the nesting.
			if (escapeNext)
			{
				escapeNext = false;
			}
			else if (inStringLiteral && c == '\\')
			{
				escapeNext = true;
			}
			else if (c == '"')
			{
				inStringLiteral = !inStringLiteral;
			}
			else if (!inStringLiteral && c == '{')
			{
				braceCount++;
			}
			else if (!inStringLiteral && c == '}')
			{
				braceCount--;

				// Complete JSON object received
				if (braceCount == 0)
				{
					serialInputBuffer[serialInputPos] = '\0';

					cJSON *doc = cJSON_Parse(serialInputBuffer);
					// print in case we are in debug mode - this is useful to see the incoming JSON in the serial monitor without the need of a separate tool
					log_i("Received JSON: %s", serialInputBuffer);

					// Reset before dispatching so an early `continue` or a
					// failed parse can never leave the frame state half-open.
					resetSerialFrame();

					if (doc)
					{
						log_i("Parsed JSON successfully");
						addJsonToQueue(doc);
					}
					else
					{
						log_w("Failed to parse JSON - ignoring line");
						reportSerialError("Failed to parse JSON");
					}
				}
			}
		}

		// Drop a frame that stopped arriving half-way through, so a sender that
		// died mid-write cannot keep the parser busy forever.
		if (inJsonObject && (millis() - frameLastByteMs) > SERIAL_FRAME_TIMEOUT_MS)
		{
			log_w("Serial frame timed out after %u bytes - discarding",
				  (unsigned)serialInputPos);
			resetSerialFrame();
			reportSerialError("Malformed JSON - frame timeout");
		}

		// Let other tasks run
		vTaskDelay(pdMS_TO_TICKS(5)); // Reduced from 10ms for faster processing
	}

	void serialize(cJSON *doc)
	{
		// Use the new thread-safe function
		serial_out_safe_serialize_json(doc);
		// IMPORTANT: Only delete the doc if it's a new object created by a controller
		// The serialTask is responsible for deleting the root request object
		// This function is called with RESPONSE objects created by controllers (get/act functions)
		if (doc != NULL)
		{
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

		// T4.1: build the "++\n<json>\n--\n" message and enqueue via the
		// pure-C output path (serial_out.c).
		serial_out_safe_send_json_string(s);
		free(s);
	}

	void jsonProcessor(char *task, cJSON *jsonDocument)
	{
		log_i("Processing task: %s", task);

		// Route commands through RoutingTable — handles LOCAL, REMOTE, OFF
		// transparently for all roles (master, slave, standalone).
		log_i("Routing task through DeviceRouter");
		cJSON *canResponse = DeviceRouter::routeCommand(task, jsonDocument);
		if (canResponse)
		{
			log_i("Received response from DeviceRouter for task %s", task);
			serialize(canResponse);
			return; // If the command was routed, we assume it was handled and return immediately. If the command is not recognized by the router, it returns nullptr and we continue with local processing.
		}

		/*
		This function takes in the task (e.g. /state_get)
		and a JSON object that holds the information for the
		task that needs to be processed. It calls the get/act
		function for the different controllers*/
		/* KEEP THAT!! */ if (false) // keep all other else ifs happy
			return;
#ifdef ANALOG_OUT_CONTROLLER
		else if (runtimeConfig.analogOut && strcmp(task, analogout_act_endpoint) == 0)
			serialize(AnalogOutController::act(jsonDocument));
		else if (runtimeConfig.analogOut && strcmp(task, analogout_get_endpoint) == 0)
			serialize(AnalogOutController::get(jsonDocument));
#endif
#ifdef BLUETOOTH
		else if (runtimeConfig.bluetooth && strcmp(task, bt_connect_endpoint) == 0)
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
		else if (runtimeConfig.bluetooth && strcmp(task, bt_scan_endpoint) == 0)
		{
			BtController::scanForDevices(jsonDocument);
		}
		else if (runtimeConfig.bluetooth && strcmp(task, bt_disconnect_endpoint) == 0)
		{
			BtController::disconnect();
		}
#endif

#ifdef DAC_CONTROLLER
		else if (runtimeConfig.dac && strcmp(task, dac_act_endpoint) == 0)
			serialize(DacController::act(jsonDocument));
		// else  if (strcmp(task, dac_get_endpoint) == 0)
		//	serialize(DacController::get(jsonDocument));
#endif

#ifdef DIGITAL_IN_CONTROLLER
		else if (runtimeConfig.digitalIn && strcmp(task, digitalin_act_endpoint) == 0)
			serialize(DigitalInController::act(jsonDocument));
		else if (runtimeConfig.digitalIn && strcmp(task, digitalin_get_endpoint) == 0)
			serialize(DigitalInController::get(jsonDocument));
#endif
#ifdef DIGITAL_OUT_CONTROLLER
		// digitalout_act / digitalout_get are now routed through DeviceRouter
		// (see DeviceRouter::handleDigitalOutAct). DeviceRouter handles BOTH
		// the local-pin path (calls DigitalOutController::act) and the
		// remote-CAN path (SDO write to OD 0x2301 on the requested node).
		// The cases here only run if DeviceRouter returned nullptr — i.e.
		// the routing layer is disabled — so we keep the local fallback.
		else if (runtimeConfig.digitalOut && strcmp(task, digitalout_act_endpoint) == 0)
			serialize(DigitalOutController::act(jsonDocument));
		else if (runtimeConfig.digitalOut && strcmp(task, digitalout_get_endpoint) == 0)
			serialize(DigitalOutController::get(jsonDocument));
#endif

#ifdef I2C_MASTER
		else if (strcmp(task, i2c_get_endpoint) == 0)
			serialize(i2c_master::get(jsonDocument));
		else if (strcmp(task, i2c_act_endpoint) == 0)
			serialize(i2c_master::act(jsonDocument));
#endif
#if defined(CAN_CONTROLLER_CANOPEN) // CANopen transport — node ID management //
		else if (strcmp(task, can_get_endpoint) == 0)
			serialize(CANopenModule::get(jsonDocument));
		else if (strcmp(task, can_act_endpoint) == 0)
			serialize(CANopenModule::act(jsonDocument));
#endif
		// laser_get/_act, led_get/_act, motor_get/_act, home_get/_act,
		// tmc_get/_act, galvo_get/_act are handled by DeviceRouter (PR-7.7).
#ifdef LED_CONTROLLER
		// SignalController is only compiled when LED_CONTROLLER is enabled
		// (it shares the NeoPixel/HUB75 backend with the LED array).
		else if (strcmp(task, signal_act_endpoint) == 0 ||
		         strcmp(task, indicator_act_endpoint) == 0)
			serialize(SignalController::act(jsonDocument));
		else if (strcmp(task, signal_get_endpoint) == 0)
			serialize(SignalController::get(jsonDocument));
#endif
		else if (strcmp(task, buzzer_act_endpoint) == 0)
			serialize(BuzzerController::act(jsonDocument));
#ifdef MESSAGE_CONTROLLER
		else if (runtimeConfig.message && strcmp(task, message_get_endpoint) == 0)
			serialize(MessageController::get(jsonDocument));
		else if (runtimeConfig.message && strcmp(task, message_act_endpoint) == 0)
			serialize(MessageController::act(jsonDocument));
#endif
#ifdef SCANNER_CONTROLLER //TODO: When will this case actually be reached? probably we will always go through the routing? 	
		else if (runtimeConfig.scanner && strcmp(task, scanner_get_endpoint) == 0)
			serialize(ScannerController::get(jsonDocument));
		else if (runtimeConfig.scanner && strcmp(task, scanner_act_endpoint) == 0)
			serialize(ScannerController::act(jsonDocument));
#endif
#ifdef PID_CONTROLLER
		else if (runtimeConfig.pid && strcmp(task, PID_get_endpoint) == 0)
			serialize(PidController::get(jsonDocument));
		else if (runtimeConfig.pid && strcmp(task, PID_act_endpoint) == 0)
			serialize(PidController::act(jsonDocument));
#endif

		else if (strcmp(task, state_act_endpoint) == 0)
			// Handled by DeviceRouter::handleStateAct (PR-7.8). Should never
			// reach here; kept as a safety net so a stray state_act doesn't
			// silently fall through.
			serialize(State::act(jsonDocument));
		else if (strcmp(task, state_get_endpoint) == 0)
			serialize(State::get(jsonDocument));
		else if (strcmp(task, state_busy_endpoint) == 0)
		{
			// super fast check if the system is busy {"task":"/b"}
			// Build message and send through queue
			char buffer[32];
			snprintf(buffer, sizeof(buffer), "++{'b':%d}--", State::getBusy());
			serial_out_safe_println(buffer);
		}
		else if (strcmp(task, modules_get_endpoint) == 0)
		{
			serialize(State::getModules());
		}
		else if (strcmp(task, route_get_endpoint) == 0)
		{
			serialize(UC2::RoutingTable::toJson());
		}
		else if (strcmp(task, route_set_endpoint) == 0)
		{
			// {"task":"/route_set","type":"MOTOR","id":0,"where":"REMOTE","nodeId":10}
			cJSON *typeItem = cJSON_GetObjectItem(jsonDocument, "type");
			const char *typeStr = (typeItem && cJSON_IsString(typeItem)) ? typeItem->valuestring : nullptr;
			int logicalId = cJsonTool::getJsonInt(jsonDocument, "id");
			cJSON *whereItem = cJSON_GetObjectItem(jsonDocument, "where");
			const char *whereStr = (whereItem && cJSON_IsString(whereItem)) ? whereItem->valuestring : nullptr;
			int nodeId = cJsonTool::getJsonInt(jsonDocument, "nodeId");

			UC2::RouteEntry::Type type = UC2::RouteEntry::MOTOR;
			if (typeStr)
			{
				if (strcmp(typeStr, "LASER") == 0)
					type = UC2::RouteEntry::LASER;
				else if (strcmp(typeStr, "LED") == 0)
					type = UC2::RouteEntry::LED;
				else if (strcmp(typeStr, "GALVO") == 0)
					type = UC2::RouteEntry::GALVO;
				else if (strcmp(typeStr, "HOME") == 0)
					type = UC2::RouteEntry::HOME;
				else if (strcmp(typeStr, "TMC") == 0)
					type = UC2::RouteEntry::TMC;
			}
			UC2::RouteEntry::Where where = UC2::RouteEntry::LOCAL;
			if (whereStr)
			{
				if (strcmp(whereStr, "REMOTE") == 0)
					where = UC2::RouteEntry::REMOTE;
				else if (strcmp(whereStr, "OFF") == 0)
					where = UC2::RouteEntry::OFF;
			}
			UC2::RoutingTable::set(type, logicalId, where, (uint8_t)nodeId);
			serialize(UC2::RoutingTable::toJson());
		}
#ifdef HEAT_CONTROLLER
		else if (runtimeConfig.heat && strcmp(task, heat_get_endpoint) == 0)
			serialize(HeatController::get(jsonDocument));
		else if (runtimeConfig.heat && strcmp(task, heat_act_endpoint) == 0)
			serialize(HeatController::act(jsonDocument));
		else if (runtimeConfig.heat && strcmp(task, ds18b20_get_endpoint) == 0)
			serialize(DS18b20Controller::get(jsonDocument));
		else if (runtimeConfig.heat && strcmp(task, ds18b20_act_endpoint) == 0)
			serialize(DS18b20Controller::act(jsonDocument));
#endif
#ifdef TMP102_CONTROLLER
		else if (runtimeConfig.fan && strcmp(task, temp_get_endpoint) == 0)
			serialize(Tmp102Controller::get(jsonDocument));
		else if (runtimeConfig.fan && strcmp(task, temp_act_endpoint) == 0)
			serialize(Tmp102Controller::act(jsonDocument));
#endif
#ifdef FAN_CONTROLLER
		else if (runtimeConfig.fan && strcmp(task, fan_act_endpoint) == 0)
			serialize(FanController::act(jsonDocument));
		else if (runtimeConfig.fan && strcmp(task, fan_get_endpoint) == 0)
			serialize(FanController::get(jsonDocument));
#endif
#ifdef THERMAL_CONTROLLER
		// NTC heat-sink monitoring on the illumination board. Only compiled in
		// where TMP102_CONTROLLER is not, so the endpoints cannot collide.
		else if (strcmp(task, temp_get_endpoint) == 0)
			serialize(ThermalController::get(jsonDocument));
		else if (strcmp(task, temp_act_endpoint) == 0)
			serialize(ThermalController::act(jsonDocument));
#endif
		else if (strcmp(task, qid_state_endpoint) == 0)
			serialize(qid_handle_state_query(jsonDocument));
		else if (strcmp(task, qid_pause_endpoint) == 0)
			serialize(qid_handle_pause(jsonDocument));
		else if (strcmp(task, qid_resume_endpoint) == 0)
			serialize(qid_handle_resume(jsonDocument));
		// ── RuntimeConfig endpoints ─────────────────────────────────
		else if (strcmp(task, config_get_endpoint) == 0)
		{
			// Return current runtime configuration as JSON
			cJSON *cfg = nvsconfig_to_json();
			serialize(cfg);
		}
		else if (strcmp(task, config_set_endpoint) == 0)
		{
			// Merge partial JSON into runtimeConfig and persist to NVS
			nvsconfig_from_json(jsonDocument);
			nvsconfig_save();
			cJSON *resp = cJSON_CreateObject();
			cJSON_AddNumberToObject(resp, "success", 1);
			cJSON_AddStringToObject(resp, "msg", "config saved, reboot to apply");
			serialize(resp);
		}
		else if (strcmp(task, config_reset_endpoint) == 0)
		{
			// Clear NVS config namespace and reboot
			nvsconfig_reset();
			cJSON *resp = cJSON_CreateObject();
			cJSON_AddNumberToObject(resp, "success", 1);
			cJSON_AddStringToObject(resp, "msg", "config reset, rebooting");
			serialize(resp);
			delay(500);
			ESP.restart();
		}
		else
		{
			// Unknown task
			serialize(-1);
		}
	}
}
