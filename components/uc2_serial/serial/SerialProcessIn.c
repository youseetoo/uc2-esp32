// SerialProcessIn.c — T4.2: serial INPUT side in pure C (was SerialProcessIn.cpp).
// Byte-wise serial framing (serial_in_loop), input queue + serial task
// (serial_in_setup / serialTask), JSON dispatch (jsonProcessor), and the
// serialize() wrappers (serial_in_serialize_doc / serial_in_serialize_qid)
// that feed the pure-C output path (uc2_serial_out.c, T4.1).
//
// Public C API is SerialProcess.h (serial_in_*). The C++ `namespace
// SerialProcess` shim in the header forwards to these, so existing C++
// callers (uc2_app main.cpp) compile unchanged.
//
// C++ -> C mapping for this port:
//   SerialProcess::setup/loop/addJsonToQueue -> serial_in_setup / serial_in_loop /
//     serial_in_add_json_to_queue
//   serialize(cJSON*)/serialize(int)         -> serial_in_serialize_doc /
//     serial_in_serialize_qid
//   Serial.setTimeout/available/read         -> uc2_serial_set_timeout /
//     uc2_serial_available / uc2_serial_read (uc2_serial_c.h)
//   Serial.println (setup banner)            -> serial_out_safe_println
//   millis/delay                             -> uc2_time.h (static inline)
//   ESP.restart()                            -> esp_restart() (esp_system.h)
//   DeviceRouter::routeCommand               -> device_router_route_command
//   CANopenModule::get/act                   -> canopen_get/canopen_act
//   OtaBinaryReceive::isActive/processBytes  -> ota_isActive/ota_processBytes
//   UC2::RoutingTable::toJson/set            -> routing_table_to_json/routing_table_set
//   UC2::RouteEntry::{MOTOR,LOCAL,...}       -> ROUTE_ENTRY_TYPE_*/ROUTE_ENTRY_WHERE_*
//   cJsonTool::getJsonInt                    -> cjson_tool_get_int
//   BtController::setMacAndConnect/scanForDevices/disconnect/connectPsxController
//                                            -> bt_* (BtController.h declares the
//                                               C shims but includes C++ headers, so
//                                               they are re-declared extern here)
// Dropped: the dead processJsonDocument() helper (no callers — grep-verified).
// Omitted: the ANALOG_OUT / DAC / SCANNER dispatch cases — unreachable in this
// IDF tree (those controller headers are PIO-only and absent from components/).
#include <PinConfig.h>
#include "SerialProcess.h"
#include "Endpoints.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>    // snprintf
#include <stdlib.h>   // free
#include <string.h>   // strcmp
#include "cJSON.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_serial_c.h"
#include "uc2_hal/uc2_time.h"
#include "freertos/FreeRTOS.h" // vTaskDelay / pdMS_TO_TICKS
#include "freertos/task.h"     // xTaskCreate / vTaskDelete
#include "freertos/queue.h"    // QueueHandle_t / xQueueCreate / xQueueSend / xQueueReceive
#include "esp_system.h"        // esp_restart
#include "RuntimeConfig.h"
#include "NVSConfig.h"
#include "State.h"
#include "BuzzerController.h"
#include "QidRegistry.h"
#include "DeviceRouter.h"
#include "RoutingTable.h"
#include "cJsonTool.h"
#ifdef DIGITAL_IN_CONTROLLER
#include "DigitalInController.h"
#endif
#ifdef DIGITAL_OUT_CONTROLLER
#include "DigitalOutController.h"
#endif
#ifdef MESSAGE_CONTROLLER
#include "MessageController.h"
#endif
#ifdef LED_CONTROLLER
#include "SignalController.h" // signal_* (the LED array itself routes via DeviceRouter)
#endif
#ifdef CAN_CONTROLLER_CANOPEN
#include "CANopenModule.h"
#ifndef CANOPEN_OTA_NO_SENDER
#include "OtaBinaryReceive.h"
#endif
#endif
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
#ifdef PID_CONTROLLER
#include "PidController.h"
#endif
#ifdef BLUETOOTH
// BtController.h declares the bt_* C shims but includes C++ headers
// (HidGamePad.h, ...), so re-declare them for this C translation unit.
extern void bt_setMacAndConnect(char *mac);
#ifdef PSXCONTROLLER
extern void bt_connectPsxController(char *mac, int type);
#endif
extern cJSON *bt_scanForDevices(cJSON *doc);
extern void bt_disconnect(void);
#endif

// T1.3: tag for the uc2 log_* facade (ESP_LOG* forwarding, see uc2_log.h).
static const char *TAG = "UC2_SERIAL";

// Queue-based processing to handle serial load without blocking main thread
static QueueHandle_t serialMSGQueue = NULL; // Queue that buffers incoming messages and delegates them to the appropriate task
static TaskHandle_t xHandle = NULL;         // Task handle for the serial task

// Static buffer for serial reading to avoid heap fragmentation
static char serialInputBuffer[8192]; // 8KB buffer for large JSON strings
static size_t serialInputPos = 0;
static bool inJsonObject = false;
static int braceCount = 0;
static bool inStringLiteral = false; // inside a "..." — braces there don't nest
static bool escapeNext = false;      // previous char was a backslash in a string
static uint32_t frameLastByteMs = 0; // last time a byte of this frame arrived

// A frame that stops arriving mid-object is dropped after this long. Covers
// a sender that dies (or never terminates the line at all) — without it the
// partial frame would sit in the buffer and swallow the next command.
static const uint32_t SERIAL_FRAME_TIMEOUT_MS = 2000;

void serial_in_serialize_doc(cJSON *doc)
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

// Answer a bad line with a JSON error rather than silently dropping it.
static void reportSerialError(const char *reason)
{
    cJSON *errorResponse = cJSON_CreateObject();
    if (errorResponse != NULL)
    {
        cJSON_AddStringToObject(errorResponse, "error", reason);
        serial_in_serialize_doc(errorResponse); // serial_in_serialize_doc() takes ownership
    }
}

void serial_in_serialize_qid(int qid)
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
    // pure-C output path (uc2_serial_out.c).
    serial_out_safe_send_json_string(s);
    free(s);
}

static void jsonProcessor(char *task, cJSON *jsonDocument)
{
    log_i("Processing task: %s", task);

    // Route commands through RoutingTable — handles LOCAL, REMOTE, OFF
    // transparently for all roles (master, slave, standalone).
    log_i("Routing task through DeviceRouter");
    cJSON *canResponse = device_router_route_command(task, jsonDocument);
    if (canResponse)
    {
        log_i("Received response from DeviceRouter for task %s", task);
        serial_in_serialize_doc(canResponse);
        return; // If the command was routed, we assume it was handled and return immediately. If the command is not recognized by the router, it returns nullptr and we continue with local processing.
    }

    /*
    This function takes in the task (e.g. /state_get)
    and a JSON object that holds the information for the
    task that needs to be processed. It calls the get/act
    function for the different controllers*/
    /* KEEP THAT!! */ if (false) // keep all other else ifs happy
        return;
#ifdef BLUETOOTH
    else if (runtimeConfig.bluetooth && strcmp(task, bt_connect_endpoint) == 0)
    {
        // {"task":"/bt_connect", "mac":"1a:2b:3c:01:01:01", "psx":2}
        char *mac = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(jsonDocument, "mac")); // jsonDocument["mac"];
        int ps = cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(jsonDocument, "psx"));    // jsonDocument["psx"];
#ifdef BTHID
        bt_setMacAndConnect(mac);
#endif
#ifdef PSXCONTROLLER
        bt_connectPsxController(mac, ps);
#endif
    }
    else if (runtimeConfig.bluetooth && strcmp(task, bt_scan_endpoint) == 0)
    {
        bt_scanForDevices(jsonDocument);
    }
    else if (runtimeConfig.bluetooth && strcmp(task, bt_disconnect_endpoint) == 0)
    {
        bt_disconnect();
    }
#endif
#ifdef DIGITAL_IN_CONTROLLER
    else if (runtimeConfig.digitalIn && strcmp(task, digitalin_act_endpoint) == 0)
        serial_in_serialize_qid(digitalin_act(jsonDocument));
    else if (runtimeConfig.digitalIn && strcmp(task, digitalin_get_endpoint) == 0)
        serial_in_serialize_doc(digitalin_get(jsonDocument));
#endif
#ifdef DIGITAL_OUT_CONTROLLER
    // digitalout_act / digitalout_get are now routed through DeviceRouter
    // (see DeviceRouter::handleDigitalOutAct). The RoutingTable decides
    // LOCAL / REMOTE / OFF per logical DO ID and forwards REMOTE commands
    // as CANopen SDO writes. The local fallback below is kept as a safety
    // net for commands the router doesn't recognize (e.g. unknown task).
    else if (runtimeConfig.digitalOut && strcmp(task, digitalout_act_endpoint) == 0)
        serial_in_serialize_qid(digitalout_act(jsonDocument));
    else if (runtimeConfig.digitalOut && strcmp(task, digitalout_get_endpoint) == 0)
        serial_in_serialize_doc(digitalout_get(jsonDocument));
#endif

#if defined(CAN_CONTROLLER_CANOPEN) // CANopen transport — node ID management //
    else if (strcmp(task, can_get_endpoint) == 0)
        serial_in_serialize_doc(canopen_get(jsonDocument));
    else if (strcmp(task, can_act_endpoint) == 0)
        serial_in_serialize_doc(canopen_act(jsonDocument));
#endif
    // laser_get/_act, led_get/_act, motor_get/_act, home_get/_act,
    // tmc_get/_act, galvo_get/_act are handled by DeviceRouter (PR-7.7).
#ifdef LED_CONTROLLER
    // SignalController is only compiled when LED_CONTROLLER is enabled
    // (it shares the NeoPixel/HUB75 backend with the LED array).
    else if (strcmp(task, signal_act_endpoint) == 0 ||
             strcmp(task, indicator_act_endpoint) == 0)
        serial_in_serialize_qid(signal_act(jsonDocument));
    else if (strcmp(task, signal_get_endpoint) == 0)
        serial_in_serialize_doc(signal_get(jsonDocument));
#endif
    else if (strcmp(task, buzzer_act_endpoint) == 0)
        serial_in_serialize_qid(buzzer_act(jsonDocument));
#ifdef MESSAGE_CONTROLLER
    else if (runtimeConfig.message && strcmp(task, message_get_endpoint) == 0)
        serial_in_serialize_doc(message_get(jsonDocument));
    else if (runtimeConfig.message && strcmp(task, message_act_endpoint) == 0)
        serial_in_serialize_qid(message_act(jsonDocument));
#endif
#ifdef PID_CONTROLLER
    else if (runtimeConfig.pid && strcmp(task, PID_get_endpoint) == 0)
        serial_in_serialize_doc(pid_get(jsonDocument));
    else if (runtimeConfig.pid && strcmp(task, PID_act_endpoint) == 0)
        serial_in_serialize_qid(pid_act(jsonDocument));
#endif

    else if (strcmp(task, state_act_endpoint) == 0)
        // Handled by DeviceRouter::handleStateAct (PR-7.8). Should never
        // reach here; kept as a safety net so a stray state_act doesn't
        // silently fall through.
        serial_in_serialize_qid(state_act(jsonDocument));
    else if (strcmp(task, state_get_endpoint) == 0)
        serial_in_serialize_doc(state_get(jsonDocument));
    else if (strcmp(task, state_busy_endpoint) == 0)
    {
        // super fast check if the system is busy {"task":"/b"}
        // Build message and send through queue
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "++{'b':%d}--", (int)state_get_busy());
        serial_out_safe_println(buffer);
    }
    else if (strcmp(task, modules_get_endpoint) == 0)
    {
        serial_in_serialize_doc(state_get_modules());
    }
    else if (strcmp(task, route_get_endpoint) == 0)
    {
        serial_in_serialize_doc(routing_table_to_json());
    }
    else if (strcmp(task, route_set_endpoint) == 0)
    {
        // {"task":"/route_set","type":"MOTOR","id":0,"where":"REMOTE","nodeId":10}
        cJSON *typeItem = cJSON_GetObjectItem(jsonDocument, "type");
        const char *typeStr = (typeItem && cJSON_IsString(typeItem)) ? typeItem->valuestring : NULL;
        int logicalId = cjson_tool_get_int(jsonDocument, "id", 0);
        cJSON *whereItem = cJSON_GetObjectItem(jsonDocument, "where");
        const char *whereStr = (whereItem && cJSON_IsString(whereItem)) ? whereItem->valuestring : NULL;
        int nodeId = cjson_tool_get_int(jsonDocument, "nodeId", 0);

        route_entry_type_t type = ROUTE_ENTRY_TYPE_MOTOR;
        if (typeStr)
        {
            if (strcmp(typeStr, "LASER") == 0)
                type = ROUTE_ENTRY_TYPE_LASER;
            else if (strcmp(typeStr, "LED") == 0)
                type = ROUTE_ENTRY_TYPE_LED;
            else if (strcmp(typeStr, "GALVO") == 0)
                type = ROUTE_ENTRY_TYPE_GALVO;
            else if (strcmp(typeStr, "HOME") == 0)
                type = ROUTE_ENTRY_TYPE_HOME;
            else if (strcmp(typeStr, "TMC") == 0)
                type = ROUTE_ENTRY_TYPE_TMC;
        }
        route_entry_where_t where = ROUTE_ENTRY_WHERE_LOCAL;
        if (whereStr)
        {
            if (strcmp(whereStr, "REMOTE") == 0)
                where = ROUTE_ENTRY_WHERE_REMOTE;
            else if (strcmp(whereStr, "OFF") == 0)
                where = ROUTE_ENTRY_WHERE_OFF;
        }
        // C shim: subAxis defaults to 0 (as the C++ 5-arg overload did).
        routing_table_set(type, (uint8_t)logicalId, where, (uint8_t)nodeId, 0);
        serial_in_serialize_doc(routing_table_to_json());
    }
#ifdef HEAT_CONTROLLER
    else if (runtimeConfig.heat && strcmp(task, heat_get_endpoint) == 0)
        serial_in_serialize_doc(heat_get(jsonDocument));
    else if (runtimeConfig.heat && strcmp(task, heat_act_endpoint) == 0)
        serial_in_serialize_qid(heat_act(jsonDocument));
    else if (runtimeConfig.heat && strcmp(task, ds18b20_get_endpoint) == 0)
        serial_in_serialize_doc(ds18b20_get(jsonDocument));
    else if (runtimeConfig.heat && strcmp(task, ds18b20_act_endpoint) == 0)
        serial_in_serialize_qid(ds18b20_act(jsonDocument));
#endif
#ifdef TMP102_CONTROLLER
    else if (runtimeConfig.fan && strcmp(task, temp_get_endpoint) == 0)
        serial_in_serialize_doc(tmp102_get(jsonDocument));
    else if (runtimeConfig.fan && strcmp(task, temp_act_endpoint) == 0)
        serial_in_serialize_qid(tmp102_act(jsonDocument));
#endif
#ifdef FAN_CONTROLLER
    else if (runtimeConfig.fan && strcmp(task, fan_act_endpoint) == 0)
        serial_in_serialize_qid(fan_act(jsonDocument));
    else if (runtimeConfig.fan && strcmp(task, fan_get_endpoint) == 0)
        serial_in_serialize_doc(fan_get(jsonDocument));
#endif
#ifdef THERMAL_CONTROLLER
    // NTC heat-sink monitoring on the illumination board. Only compiled in
    // where TMP102_CONTROLLER is not, so the endpoints cannot collide.
    else if (strcmp(task, temp_get_endpoint) == 0)
        serial_in_serialize_doc(thermal_get(jsonDocument));
    else if (strcmp(task, temp_act_endpoint) == 0)
        serial_in_serialize_qid(thermal_act(jsonDocument));
#endif
    else if (strcmp(task, qid_state_endpoint) == 0)
        serial_in_serialize_qid(qid_handle_state_query(jsonDocument));
    else if (strcmp(task, qid_pause_endpoint) == 0)
        serial_in_serialize_qid(qid_handle_pause(jsonDocument));
    else if (strcmp(task, qid_resume_endpoint) == 0)
        serial_in_serialize_qid(qid_handle_resume(jsonDocument));
    // ── RuntimeConfig endpoints ─────────────────────────────────
    else if (strcmp(task, config_get_endpoint) == 0)
    {
        // Return current runtime configuration as JSON
        cJSON *cfg = nvsconfig_to_json();
        serial_in_serialize_doc(cfg);
    }
    else if (strcmp(task, config_set_endpoint) == 0)
    {
        // Merge partial JSON into runtimeConfig and persist to NVS
        nvsconfig_from_json(jsonDocument);
        nvsconfig_save();
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "success", 1);
        cJSON_AddStringToObject(resp, "msg", "config saved, reboot to apply");
        serial_in_serialize_doc(resp);
    }
    else if (strcmp(task, config_reset_endpoint) == 0)
    {
        // Clear NVS config namespace and reboot
        nvsconfig_reset();
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "success", 1);
        cJSON_AddStringToObject(resp, "msg", "config reset, rebooting");
        serial_in_serialize_doc(resp);
        delay(500);
        esp_restart();
    }
    else
    {
        // Unknown task
        serial_in_serialize_qid(-1);
    }
}

// Reset the serial input buffer state
static void resetSerialFrame(void)
{
    serialInputPos = 0;
    inJsonObject = false;
    braceCount = 0;
    inStringLiteral = false;
    escapeNext = false;
    frameLastByteMs = 0;
}

static void serialTask(void *p)
{
    (void)p;
    for (;;)
    {
        cJSON *root = NULL;

        // Wait for a message
        if (xQueueReceive(serialMSGQueue, &root, portMAX_DELAY) == pdTRUE)
        {
            if (root)
            {
                // Process the message
                char *task = NULL;
                cJSON *task_item = cJSON_GetObjectItemCaseSensitive(root, "task");
                if (cJSON_IsString(task_item) && (task_item->valuestring != NULL))
                    task = task_item->valuestring;

                if (task)
                {
                    log_i("Processing serial task: %s", task);

                    // Check if we should process this message
                    // (filtering can be added here if needed)
                    bool shouldProcess = true; // Simplified: always process

                    if (shouldProcess)
                    {
                        // Process the JSON document
                        jsonProcessor(task, root);
                        // IMPORTANT: Delete the root object after processing to prevent memory leak
                        // The jsonProcessor should not keep references to this object after it returns
                        cJSON_Delete(root);
                        root = NULL; // Prevent double-free
                    }
                }
                else
                {
                    log_w("Received serial message without valid task field");
                    // Still delete the object to prevent memory leak
                    cJSON_Delete(root);
                    root = NULL;
                }
            }
        }
    }
    vTaskDelete(NULL);
}

// This function initializes the serial input system
// It sets up the queue, task, and any other necessary resources
void serial_in_setup(void)
{
    // T4.1: initialise the pure-C thread-safe serial output (output queue +
    // output task) before the input path can serialize anything. The output
    // task runs one priority above the default (was +1 in the C++ version).
    serial_out_init((UBaseType_t)(pinConfig.DEFAULT_TASK_PRIORITY + 1));
    serial_out_safe_println("++{\"version\":\"1.0.0\"}--");

    // Create a queue to hold incoming serial messages (cJSON objects)
    // Queue size 10 means it can hold 10 messages before blocking
    // Each message is a pointer (4 bytes on ESP32)
    serialMSGQueue = xQueueCreate(10, sizeof(cJSON *));

    // Create a task to process messages from the queue
    // Priority 2 (higher than most, but not as high as critical tasks)
    // Stack size 4096 bytes (enough for JSON parsing)
    // The task function is serialTask which processes messages from the queue
    // The parameter passed to the task is NULL (not used in this case)
    xTaskCreate(serialTask, "serialTask", pinConfig.BT_CONTROLLER_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, &xHandle);

    // Set serial timeout to 0 (non-blocking) for fast polling
    // This allows the main loop to run without delays
    uc2_serial_set_timeout(0);
}

// This function handles serial input in the main loop
void serial_in_loop(void)
{
#if defined(CAN_CONTROLLER_CANOPEN) && !defined(CANOPEN_OTA_NO_SENDER)
    // CANopen path: /ota_start preamble flips this on. Every byte consumed by
    // processBytes() until /ota_done or a 5s idle timeout (see
    // OtaBinaryReceive.h). The byte loop below is skipped for this period so
    // hex/OTA payload bytes are not treated as JSON commands.
    if (ota_isActive())
    {
        ota_processBytes();
        vTaskDelay(pdMS_TO_TICKS(1));
        return;
    }
#endif

    // Read serial data byte-by-byte to handle large JSON objects
    // This approach avoids buffer size limitations and handles any size
    // The byte-by-byte approach also prevents blocking the main thread
    // for long periods, improving system responsiveness
    while (uc2_serial_available() > 0)
    {
        int c = uc2_serial_read();

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
        if (serialInputPos < sizeof(serialInputBuffer) - 1)
        {
            serialInputBuffer[serialInputPos++] = (char)c;
        }
        else
        {
            log_w("Serial input buffer overflow (%u bytes) - discarding",
                  (unsigned)sizeof(serialInputBuffer));
            resetSerialFrame();
            reportSerialError("Input buffer overflow");
            continue;
        }

        // Track JSON object boundaries byte-by-byte
        // This ensures we only process complete JSON objects, even if
        // multiple arrive in one line or the line is very long
        if (!inJsonObject)
        {
            // Not in an object, check if we're starting one
            if (c == '{')
            {
                inJsonObject = true;
                braceCount = 1;
                inStringLiteral = false;
                escapeNext = false;
                frameLastByteMs = millis();
            }
        }
        else
        {
            // Inside a JSON object, track brace depth and string literals
            if (escapeNext)
            {
                // This byte was escaped (e.g. \") — don't interpret it
                escapeNext = false;
            }
            else if (c == '\\')
            {
                // Backslash — next byte is escaped (only meaningful in strings)
                escapeNext = inStringLiteral;
            }
            else if (c == '"')
            {
                // Toggle string literal state
                inStringLiteral = !inStringLiteral;
            }
            else if (!inStringLiteral)
            {
                // Only count braces outside of string literals
                if (c == '{')
                {
                    braceCount++;
                }
                else if (c == '}')
                {
                    braceCount--;
                    if (braceCount == 0)
                    {
                        // Complete JSON object received
                        serialInputBuffer[serialInputPos] = '\0';
                        // Add to queue for processing
                        serial_in_add_json_to_queue(cJSON_Parse(serialInputBuffer));
                        resetSerialFrame();
                    }
                }
            }
            frameLastByteMs = millis();
        }
    }

    // Drop a stalled frame: bytes stopped arriving mid-object.
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

// Add a JSON object directly to the processing queue
// This is useful for adding messages from other sources (e.g., I2C)
// or for testing without actual serial input
// Note: The serial task will take ownership of the document and delete it
// after processing. Do NOT delete the document after calling this function.
void serial_in_add_json_to_queue(cJSON *doc)
{
    if (doc)
    {
        // Try to add to queue with a timeout of 100ms
        // If the queue is full after 100ms, discard the message
        if (xQueueSend(serialMSGQueue, &doc, pdMS_TO_TICKS(100)) != pdTRUE)
        {
            // Queue full, discard message to prevent memory leak
            log_e("Serial queue full, discarding message");
            reportSerialError("Serial queue full");
            cJSON_Delete(doc);
        }
    }
}
