// T4.1: pure-C serial OUTPUT path (extracted from the C++ SerialProcess
// namespace). See uc2_serial_out.h for the API. Behaviour and constants
// (queue depth, task stack, send timeout, 8 KB cap) match the original C++
// implementation exactly.
#include "uc2_serial_out.h"

#include <stdlib.h>  // malloc/free
#include <string.h>  // strlen/strcpy/strcat/memcpy

#include "freertos/queue.h"
#include "freertos/task.h"

#include "uc2_hal/uc2_log.h"       // log_e/log_w (C-compatible macros)
#include "uc2_hal/uc2_serial_c.h"  // C shim over the C++ uc2::Serial_

// Max size of a single output message (matches the C++ SerialProcess: 8192).
#define SERIAL_OUT_MAX_LEN 8192
// Max messages queued before we drop (matches the C++ SerialProcess: 10).
#define SERIAL_OUT_QUEUE_LEN 10
// Output task stack size (matches the C++ SerialProcess: 3072).
#define SERIAL_OUT_TASK_STACK 3072
// Queue send timeout in ms (matches the C++ SerialProcess: 100).
#define SERIAL_OUT_SEND_TIMEOUT_MS 100

// Structure to hold a serial output message.
typedef struct
{
    char *message;
    size_t length;
} SerialOutMessage;

// uc2_log.h macros expand to ESP_LOG*(TAG, ...); C files have no class-scope
// TAG, so define one per translation unit.
static const char *TAG = "UC2_SERIAL_OUT";

// Queue + task for the serial output (file-static: single output path).
static QueueHandle_t serial_output_queue = NULL;
static TaskHandle_t serial_output_task_handle = NULL;

// The serial output task — processes queued output to prevent race conditions.
static void serial_out_task(void *p)
{
    (void)p;
    for (;;)
    {
        SerialOutMessage msg;
        if (xQueueReceive(serial_output_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            if (msg.message != NULL)
            {
                // Direct serial write, no critical section — this task has
                // exclusive access to the output path.
                uc2_serial_write_bytes((const uint8_t *)msg.message, msg.length);
                uc2_serial_flush();
                free(msg.message);
            }
        }
    }
    vTaskDelete(NULL);
}

void serial_out_init(UBaseType_t priority)
{
    if (serial_output_queue == NULL)
        serial_output_queue = xQueueCreate(SERIAL_OUT_QUEUE_LEN, sizeof(SerialOutMessage));
    if (serial_output_queue != NULL && serial_output_task_handle == NULL)
        xTaskCreate(serial_out_task, "serialout", SERIAL_OUT_TASK_STACK, NULL, priority,
                    &serial_output_task_handle);
}

// Internal: enqueue a heap buffer; takes ownership of `message` (frees it on
// failure / hand-off to the task).
static bool serial_out_enqueue(char *message, size_t length)
{
    if (message == NULL || serial_output_queue == NULL)
    {
        free(message);
        return false;
    }
    SerialOutMessage msg;
    msg.message = message;
    msg.length = length;
    if (xQueueSend(serial_output_queue, &msg, pdMS_TO_TICKS(SERIAL_OUT_SEND_TIMEOUT_MS)) != pdTRUE)
    {
        free(message);
        log_w("Serial output queue full - dropping message");
        return false;
    }
    return true;
}

bool serial_out_safe_send_json_string(const char *json_string)
{
    if (json_string == NULL)
    {
        serial_out_safe_println("{\"error\":\"NULL JSON string\"}");
        return false;
    }
    const size_t len = strlen(json_string);
    if (len == 0 || len > SERIAL_OUT_MAX_LEN)
    {
        serial_out_safe_println("{\"error\":\"JSON string too large or empty\"}");
        return false;
    }
    // "++\n" (3) + json_string (len) + "\n--\n" (4) + NUL (1)
    const size_t total_len = len + 8;
    char *buffer = (char *)malloc(total_len);
    if (buffer == NULL)
    {
        serial_out_safe_println("{\"error\":\"Out of memory\"}");
        return false;
    }
    strcpy(buffer, "++\n");
    strcat(buffer, json_string);
    strcat(buffer, "\n--\n");
    return serial_out_enqueue(buffer, strlen(buffer) + 1);
}

bool serial_out_safe_println(const char *message)
{
    if (message == NULL)
        return false;
    const size_t len = strlen(message);
    if (len == 0 || len > SERIAL_OUT_MAX_LEN)
        return false;
    char *buffer = (char *)malloc(len + 2);
    if (buffer == NULL)
    {
        log_e("Failed to allocate serial output buffer");
        return false;
    }
    memcpy(buffer, message, len);
    buffer[len] = '\n';
    buffer[len + 1] = '\0';
    return serial_out_enqueue(buffer, len + 1);
}

void serial_out_safe_serialize_json(const cJSON *doc)
{
    if (doc == NULL)
    {
        serial_out_safe_println("{\"error\":\"NULL JSON document\"}");
        return;
    }
    // cJSON_PrintUnformatted takes a const cJSON* and returns a heap string
    // that we own (free below).
    char *s = cJSON_PrintUnformatted(doc);
    if (s == NULL)
    {
        serial_out_safe_println("{\"error\":\"Failed to serialize JSON\"}");
        return;
    }
    serial_out_safe_send_json_string(s);
    free(s);
}

bool serial_out_enqueue_bytes(const uint8_t *message, size_t length)
{
    if (message == NULL || length == 0 || length > SERIAL_OUT_MAX_LEN)
        return false;
    char *buffer = (char *)malloc(length + 1);
    if (buffer == NULL)
    {
        log_e("Failed to allocate serial output buffer");
        return false;
    }
    memcpy(buffer, message, length);
    buffer[length] = '\0';
    return serial_out_enqueue(buffer, length);
}
