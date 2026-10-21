#pragma once
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
// T1.3: Arduino.h was pulled in only so unconverted modules could rely on
// this header transitively. Keep it for Arduino builds; raw-IDF consumers
// must include their own headers (uc2_hal.h).
#ifdef ARDUINO
#include "Arduino.h"
#endif
// T4.1: the thread-safe serial OUTPUT (safePrintln / safeSerializeJson /
// safeSendJsonString + the output queue/task) moved to the pure-C
// uc2_serial_out.h. C callers use serial_out_* directly; this header keeps
// only the INPUT side (serialTask/loop/jsonProcessor/serialize/addJsonToQueue).
#include "uc2_serial_out.h"

// T4.1: C-safe — the namespace only exists for C++ TUs; C callers use the
// serial_out_* API (uc2_serial_out.h) and the serial input side stays in
// SerialProcessIn.cpp.
#ifdef __cplusplus
namespace SerialProcess
{
    // Queue-based processing to handle serial load without blocking main thread
    extern QueueHandle_t serialMSGQueue;	// Queue that buffers incoming messages and delegates them to the appropriate task
	extern TaskHandle_t xHandle;			// Task handle for the serial task

    void jsonProcessor(char * task,cJSON * jsonDocument);
    void serialize(cJSON * doc);
    void serialize(int success);
    void serialTask(void *p);             // Serial processing task
    void setup();
    void loop();
    void addJsonToQueue(cJSON * doc);   // Add a cJSON object to the processing queue
};
#endif  // __cplusplus
