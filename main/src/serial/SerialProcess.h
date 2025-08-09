#pragma once
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Arduino.h"
#include "BinaryProtocol.h"

namespace SerialProcess
{
    //QueueHandle_t serialMSGQueue;	// Queue that buffers incoming messages and delegates them to the appropriate task
	//xTaskHandle xHandle;			// Task handle for the serial task

    void jsonProcessor(char * task,cJSON * jsonDocument);
    void serialize(cJSON * doc);
    void serialize(int success);
    void setup();
    void loop();
    void addJsonToQueue(cJSON * doc);   // Add a cJSON object to the processing queue
    
#ifdef ENABLE_BINARY_PROTOCOL
    // Binary protocol support functions
    bool detectBinaryMessage(const uint8_t* data, size_t length);
    void processBinaryData();
#endif
};

