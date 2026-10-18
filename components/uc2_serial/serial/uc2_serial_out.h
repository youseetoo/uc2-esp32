// T4.1: pure-C serial OUTPUT path (extracted from the C++ SerialProcess
// namespace). This is the "shared safeSendJsonString provider": every module
// that needs to emit a JSON line over the device serial calls these C
// functions (they enqueue to a dedicated serial output task, so the write is
// race-free and the caller never blocks on the UART).
//
// The output task writes via the C shim uc2_serial_c.h (which wraps the C++
// uc2::Serial_); the rest of this module is plain C (FreeRTOS queue + stdlib).
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"  // UBaseType_t
#include "cJSON.h"               // the cjson component is plain C (extern "C" guarded)

// Initialize the serial output queue + task. Call once from setup(); it is
// idempotent (a second call is a no-op). `priority` is the FreeRTOS task
// priority — the caller passes the board-specific value (the C++ side used to
// use pinConfig.DEFAULT_TASK_PRIORITY + 1).
void serial_out_init(UBaseType_t priority);

// Send a pre-serialized JSON string framed as "++\n<json>\n--\n". The input
// must be a NUL-terminated, valid UTF-8 JSON string; it is copied into a
// heap buffer that the output task frees. Returns true if enqueued.
bool serial_out_safe_send_json_string(const char *json_string);

// Thread-safe Serial.println: copies `message` + appends '\n' and enqueues.
bool serial_out_safe_println(const char *message);

// Serialize `doc` to a compact JSON string and send it framed (like
// serial_out_safe_send_json_string). Does NOT take ownership of `doc` — the
// caller still owns and must free it.
void serial_out_safe_serialize_json(const cJSON *doc);

// Enqueue a raw byte buffer (no framing, no newline). The buffer is copied;
// the caller keeps ownership of `message`.
bool serial_out_enqueue_bytes(const uint8_t *message, size_t length);

#ifdef __cplusplus
}
#endif
