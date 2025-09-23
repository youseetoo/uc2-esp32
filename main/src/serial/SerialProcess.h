#pragma once
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Arduino.h"

namespace SerialProcess
{
    // Disabled queue-based processing to fix ESP32S3 serial corruption
    // QueueHandle_t serialMSGQueue;	// Queue that buffers incoming messages and delegates them to the appropriate task
	// xTaskHandle xHandle;			// Task handle for the serial task

    void jsonProcessor(char * task,cJSON * jsonDocument);
    void serialize(cJSON * doc);
    void serialize(int success);
    void safeSerializeJson(cJSON * doc);  // Thread-safe JSON serialization
    void setup();
    void loop();
    void addJsonToQueue(cJSON * doc);   // Add a cJSON object to the processing queue (now processes immediately)
};

