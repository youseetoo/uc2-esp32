#pragma once
// T4.2: pure-C serial INPUT API. This file used to declare nothing — the API
// lived in `namespace SerialProcess` (SerialProcessIn.cpp). That file is now
// SerialProcessIn.c, and its public functions are the serial_in_* wrappers
// below. The thread-safe serial OUTPUT (safePrintln / safeSerializeJson /
// safeSendJsonString + the output queue/task) is the pure-C uc2_serial_out.h
// (T4.1); the input path reuses it for every response.
#include <stdbool.h>
#include "cJSON.h"
#include "uc2_serial_out.h"  // T4.1: pure-C thread-safe serial output

#ifdef __cplusplus
extern "C" {
#endif

// Create the input queue + serial task, initialise the pure-C serial output
// and the UART read timeout. Call once at boot.
void serial_in_setup(void);

// Poll serial RX (byte-wise framing) and dispatch parsed JSON. Call from the
// main loop.
void serial_in_loop(void);

// Add a cJSON object directly to the processing queue, bypassing serial RX
// (e.g. via I2C). The serial task takes ownership of the doc (frees it after
// processing, or after a send failure).
void serial_in_add_json_to_queue(cJSON *doc);

// Serialize a controller response document to the serial output, then free
// the doc (the doc is always a freshly created response object).
void serial_in_serialize_doc(cJSON *doc);

// Serialize a qid result as {"qid":n,"success":±1|0} to the serial output.
void serial_in_serialize_qid(int qid);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// C++ shim: keeps existing `SerialProcess::setup/loop/...` call sites
// compiling unchanged (uc2_app main.cpp).
namespace SerialProcess
{
    inline void setup() { serial_in_setup(); }
    inline void loop() { serial_in_loop(); }
    inline void addJsonToQueue(cJSON *doc) { serial_in_add_json_to_queue(doc); }
    inline void serialize(cJSON *doc) { serial_in_serialize_doc(doc); }
    inline void serialize(int success) { serial_in_serialize_qid(success); }
}
#endif
