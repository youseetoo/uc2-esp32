#pragma once
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Arduino.h"

namespace SerialProcess
{
    // Queue-based processing to handle serial load without blocking main thread
    extern QueueHandle_t serialMSGQueue;	// Queue that buffers incoming messages and delegates them to the appropriate task
	extern xTaskHandle xHandle;			// Task handle for the serial task
	extern QueueHandle_t serialOutputQueue;	// Queue for thread-safe serial output
	extern xTaskHandle xOutputHandle;		// Task handle for serial output task

    void jsonProcessor(char * task,cJSON * jsonDocument);
    void serialize(cJSON * doc);
    void serialize(int success);
    void safeSerializeJson(cJSON * doc);  // Thread-safe JSON serialization
    void serialTask(void *p);             // Serial processing task
    void serialOutputTask(void *p);       // Serial output task
    void safePrintln(const char* message); // Thread-safe Serial.println replacement
    void setup();
    void loop();
    void addJsonToQueue(cJSON * doc);   // Add a cJSON object to the processing queue
};

