// SerialProtocol.c — Serial Protocol V2 (MessagePack / JSON / legacy) in C.
//
// Converted from SerialProtocol.cpp (T4.2). The public API is the prefixed
// free functions declared in SerialProtocol.h; the former private class
// methods are file-local `static` functions. This translation unit is
// host-test-only (not part of the device build).
//
// C++ -> C conversions: namespace/class -> free + static functions,
// `enum class` -> C enum (values = on-wire magic bytes, unchanged),
// reference out-params -> pointers, nullptr/NULL, static_cast/reinterpret_cast
// -> C casts, and log_* -> ESP_LOG* (via esp_log.h, host stub no-ops).

#include "SerialProtocol.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"

// MessagePack (mpack) support is optional (HAS_MPACK).
#ifdef HAS_MPACK
#include <mpack.h>
#endif

static const char *TAG = "SerialProtocol";
#define log_d(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)
#define log_w(fmt, ...) ESP_LOGW(TAG, fmt, ##__VA_ARGS__)
#define log_e(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)

// File-local protocol constants (were: SerialProtocol class statics).
#define HEADER_SIZE 4
#define MAX_MESSAGE_SIZE SERIAL_PROTOCOL_MAX_MESSAGE_SIZE
#define LEGACY_START "++\n"
#define LEGACY_END "\n--\n"
#define LEGACY_START_LEN 3
#define LEGACY_END_LEN 4

// ---------------------------------------------------------------------------
// Forward declarations of the static (private) helpers.
// ---------------------------------------------------------------------------
static cJSON *decodeJson(const uint8_t *payload, size_t len);
static cJSON *decodeLegacy(const uint8_t *data, size_t len);
static cJSON *decodeMsgPack(const uint8_t *payload, size_t len);
static uint8_t *encodeJson(cJSON *doc, size_t *outLen);
static uint8_t *encodeLegacy(cJSON *doc, size_t *outLen);
static uint8_t *encodeMsgPack(cJSON *doc, size_t *outLen);
static void buildHeader(uint8_t *header, MessageFormat format, uint16_t payloadLen);
static bool parseHeader(const uint8_t *header, MessageFormat *format, uint16_t *payloadLen);
#ifdef HAS_MPACK
static cJSON *mpackNodeToJson(mpack_node_t node);
static cJSON *mpackArrayToJson(mpack_node_t node);
static cJSON *mpackMapToJson(mpack_node_t node);
static void jsonToMpack(mpack_writer_t *writer, cJSON *item);
static void jsonArrayToMpack(mpack_writer_t *writer, cJSON *array);
static void jsonObjectToMpack(mpack_writer_t *writer, cJSON *object);
#endif

//=============================================================================
// Public Interface
//=============================================================================

MessageFormat SerialProtocol_detectFormat(const uint8_t *data, size_t len)
{
    if (data == NULL || len < 1) {
        return UNKNOWN;
    }

    // Check for legacy format first
    if (len >= LEGACY_START_LEN &&
        memcmp(data, LEGACY_START, LEGACY_START_LEN) == 0) {
        return JSON;  // Legacy is still JSON
    }

    // Check magic byte for new format
    uint8_t magic = data[0];

    switch (magic) {
        case JSON:
            return JSON;
        case MSGPACK:
            return MSGPACK;
        case BINARY:
            return BINARY;
        default:
            return UNKNOWN;
    }
}

bool SerialProtocol_getMessageInfo(const uint8_t *data, size_t len,
                                   MessageFormat *format, uint16_t *payloadLen)
{
    *format = SerialProtocol_detectFormat(data, len);
    *payloadLen = 0;

    if (*format == UNKNOWN) {
        return false;
    }

    // Legacy format - find end delimiter
    if (len >= LEGACY_START_LEN &&
        memcmp(data, LEGACY_START, LEGACY_START_LEN) == 0) {
        // A complete legacy frame is at least "++\n" + "\n--\n"; anything
        // shorter cannot contain the end delimiter (and `len - LEGACY_END_LEN`
        // would underflow the size_t search bound below).
        if (len < LEGACY_START_LEN + LEGACY_END_LEN) {
            return false;  // Incomplete legacy message
        }
        // Search for the end delimiter. It may start anywhere from
        // LEGACY_START_LEN up to AND INCLUDING len - LEGACY_END_LEN (a well-
        // formed frame has the delimiter exactly at the buffer end), so the
        // bound is inclusive.
        for (size_t i = LEGACY_START_LEN; i <= len - LEGACY_END_LEN; i++) {
            if (memcmp(data + i, LEGACY_END, LEGACY_END_LEN) == 0) {
                *payloadLen = i - LEGACY_START_LEN;
                return true;
            }
        }
        return false;  // Incomplete legacy message
    }

    // New format - parse header
    if (len < HEADER_SIZE) {
        return false;  // Incomplete header
    }

    return parseHeader(data, format, payloadLen);
}

bool SerialProtocol_isMessageComplete(const uint8_t *data, size_t len)
{
    MessageFormat format;
    uint16_t payloadLen;

    if (!SerialProtocol_getMessageInfo(data, len, &format, &payloadLen)) {
        return false;
    }

    // Legacy format
    if (memcmp(data, LEGACY_START, LEGACY_START_LEN) == 0) {
        // Check if we have the end delimiter
        size_t expectedLen = LEGACY_START_LEN + payloadLen + LEGACY_END_LEN;
        return len >= expectedLen;
    }

    // New format
    size_t expectedLen = HEADER_SIZE + payloadLen;
    return len >= expectedLen;
}

cJSON *SerialProtocol_decode(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        log_e("Invalid input to decode");
        return NULL;
    }

    MessageFormat format = SerialProtocol_detectFormat(data, len);

    switch (format) {
        case MSGPACK:
            return decodeMsgPack(data + HEADER_SIZE,
                                 *(const uint16_t *)(data + 1));

        case JSON:
            // Check if legacy or new format
            if (memcmp(data, LEGACY_START, LEGACY_START_LEN) == 0) {
                return decodeLegacy(data, len);
            } else {
                return decodeJson(data + HEADER_SIZE,
                                  *(const uint16_t *)(data + 1));
            }

        default:
            log_e("Unknown message format: 0x%02X", data[0]);
            // Try legacy format as fallback
            return decodeLegacy(data, len);
    }
}

uint8_t *SerialProtocol_encode(cJSON *doc, MessageFormat format, size_t *outLen)
{
    if (doc == NULL) {
        log_e("Cannot encode NULL cJSON document");
        *outLen = 0;
        return NULL;
    }

    switch (format) {
        case MSGPACK:
            #ifdef HAS_MPACK
            return encodeMsgPack(doc, outLen);
            #else
            log_w("MessagePack not available, falling back to JSON");
            return encodeJson(doc, outLen);
            #endif

        case JSON:
            return encodeJson(doc, outLen);

        default:
            log_e("Unsupported encoding format: %d", (int)format);
            *outLen = 0;
            return NULL;
    }
}

// Test helper (exposes the private buildHeader).
void SerialProtocol_buildHeaderTest(uint8_t *header, MessageFormat format, uint16_t payloadLen)
{
    buildHeader(header, format, payloadLen);
}

//=============================================================================
// Private Decoders
//=============================================================================

static cJSON *decodeJson(const uint8_t *payload, size_t len)
{
    if (payload == NULL || len == 0 || len > MAX_MESSAGE_SIZE) {
        log_e("Invalid JSON payload: len=%d", len);
        return NULL;
    }

    // Ensure null termination for cJSON_Parse
    char *jsonStr = (char *)malloc(len + 1);
    if (jsonStr == NULL) {
        log_e("Failed to allocate memory for JSON string");
        return NULL;
    }

    memcpy(jsonStr, payload, len);
    jsonStr[len] = '\0';

    cJSON *doc = cJSON_Parse(jsonStr);
    free(jsonStr);

    if (doc == NULL) {
        log_e("Failed to parse JSON");
        return NULL;
    }

    log_d("Decoded JSON: %d bytes", len);
    return doc;
}

static cJSON *decodeLegacy(const uint8_t *data, size_t len)
{
    if (data == NULL || len < LEGACY_START_LEN + LEGACY_END_LEN) {
        return NULL;
    }

    // Find start delimiter
    const uint8_t *start = data;
    if (memcmp(start, LEGACY_START, LEGACY_START_LEN) != 0) {
        // Search for start delimiter
        for (size_t i = 0; i < len - LEGACY_START_LEN; i++) {
            if (memcmp(data + i, LEGACY_START, LEGACY_START_LEN) == 0) {
                start = data + i;
                break;
            }
        }
        if (start == data && memcmp(start, LEGACY_START, LEGACY_START_LEN) != 0) {
            log_e("Legacy format: start delimiter not found");
            return NULL;
        }
    }

    // Find end delimiter
    const uint8_t *end = NULL;
    size_t remaining = len - (start - data) - LEGACY_START_LEN;
    // The end delimiter must fit entirely in `remaining` bytes. A well-formed
    // frame has it exactly at the buffer end, so the search bound is inclusive
    // (and the guard keeps `remaining - LEGACY_END_LEN` from underflowing).
    if (remaining >= LEGACY_END_LEN) {
        for (size_t i = 0; i <= remaining - LEGACY_END_LEN; i++) {
            if (memcmp(start + LEGACY_START_LEN + i, LEGACY_END, LEGACY_END_LEN) == 0) {
                end = start + LEGACY_START_LEN + i;
                break;
            }
        }
    }

    if (end == NULL) {
        log_e("Legacy format: end delimiter not found");
        return NULL;
    }

    // Extract JSON payload
    size_t jsonLen = end - (start + LEGACY_START_LEN);
    const uint8_t *jsonStart = start + LEGACY_START_LEN;

    log_d("Decoded legacy JSON: %d bytes", jsonLen);
    return decodeJson(jsonStart, jsonLen);
}

static cJSON *decodeMsgPack(const uint8_t *payload, size_t len)
{
    #ifdef HAS_MPACK
    if (payload == NULL || len == 0 || len > MAX_MESSAGE_SIZE) {
        log_e("Invalid MessagePack payload: len=%d", len);
        return NULL;
    }

    // Parse MessagePack data
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, (const char *)payload, len);
    mpack_tree_parse(&tree);

    if (mpack_tree_error(&tree) != mpack_ok) {
        log_e("MessagePack parse error: %d", mpack_tree_error(&tree));
        mpack_tree_destroy(&tree);
        return NULL;
    }

    // Convert to cJSON
    mpack_node_t root = mpack_tree_root(&tree);
    cJSON *doc = mpackNodeToJson(root);

    mpack_tree_destroy(&tree);

    if (doc == NULL) {
        log_e("Failed to convert MessagePack to cJSON");
        return NULL;
    }

    log_d("Decoded MessagePack: %d bytes", len);
    return doc;
    #else
    (void)payload;
    (void)len;
    log_e("MessagePack support not compiled in");
    return NULL;
    #endif
}

//=============================================================================
// Private Encoders
//=============================================================================

static uint8_t *encodeJson(cJSON *doc, size_t *outLen)
{
    // Serialize cJSON to string
    char *jsonStr = cJSON_PrintUnformatted(doc);
    if (jsonStr == NULL) {
        log_e("Failed to serialize cJSON to string");
        *outLen = 0;
        return NULL;
    }

    size_t payloadLen = strlen(jsonStr);

    if (payloadLen > MAX_MESSAGE_SIZE) {
        log_e("JSON payload too large: %d > %d", payloadLen, MAX_MESSAGE_SIZE);
        free(jsonStr);
        *outLen = 0;
        return NULL;
    }

    // Allocate buffer for header + payload
    *outLen = HEADER_SIZE + payloadLen;
    uint8_t *buffer = (uint8_t *)malloc(*outLen);
    if (buffer == NULL) {
        log_e("Failed to allocate encoding buffer");
        free(jsonStr);
        *outLen = 0;
        return NULL;
    }

    // Build header
    buildHeader(buffer, JSON, payloadLen);

    // Copy payload
    memcpy(buffer + HEADER_SIZE, jsonStr, payloadLen);
    free(jsonStr);

    log_d("Encoded JSON: %d bytes (payload: %d)", *outLen, payloadLen);
    return buffer;
}

static uint8_t *encodeLegacy(cJSON *doc, size_t *outLen)
{
    // Serialize cJSON to string
    char *jsonStr = cJSON_PrintUnformatted(doc);
    if (jsonStr == NULL) {
        log_e("Failed to serialize cJSON to string");
        *outLen = 0;
        return NULL;
    }

    size_t payloadLen = strlen(jsonStr);
    *outLen = LEGACY_START_LEN + payloadLen + LEGACY_END_LEN;

    uint8_t *buffer = (uint8_t *)malloc(*outLen);
    if (buffer == NULL) {
        log_e("Failed to allocate encoding buffer");
        free(jsonStr);
        *outLen = 0;
        return NULL;
    }

    // Build message: "++\n<JSON>\n--\n"
    memcpy(buffer, LEGACY_START, LEGACY_START_LEN);
    memcpy(buffer + LEGACY_START_LEN, jsonStr, payloadLen);
    memcpy(buffer + LEGACY_START_LEN + payloadLen, LEGACY_END, LEGACY_END_LEN);

    free(jsonStr);

    log_d("Encoded legacy JSON: %d bytes", *outLen);
    return buffer;
}

static uint8_t *encodeMsgPack(cJSON *doc, size_t *outLen)
{
    #ifdef HAS_MPACK
    // Estimate buffer size (conservative: 2x JSON size)
    char *jsonStr = cJSON_PrintUnformatted(doc);
    size_t estimatedSize = jsonStr ? strlen(jsonStr) * 2 : 1024;
    free(jsonStr);

    if (estimatedSize > MAX_MESSAGE_SIZE) {
        estimatedSize = MAX_MESSAGE_SIZE;
    }

    // Allocate payload buffer
    char *payloadBuf = (char *)malloc(estimatedSize);
    if (payloadBuf == NULL) {
        log_e("Failed to allocate MessagePack buffer");
        *outLen = 0;
        return NULL;
    }

    // Initialize mpack writer
    mpack_writer_t writer;
    mpack_writer_init(&writer, payloadBuf, estimatedSize);

    // Convert cJSON to MessagePack
    jsonToMpack(&writer, doc);

    // Finish writing
    if (mpack_writer_destroy(&writer) != mpack_ok) {
        log_e("MessagePack encoding error");
        free(payloadBuf);
        *outLen = 0;
        return NULL;
    }

    size_t payloadLen = mpack_writer_buffer_used(&writer);

    // Allocate final buffer with header
    *outLen = HEADER_SIZE + payloadLen;
    uint8_t *buffer = (uint8_t *)malloc(*outLen);
    if (buffer == NULL) {
        log_e("Failed to allocate final buffer");
        free(payloadBuf);
        *outLen = 0;
        return NULL;
    }

    // Build header
    buildHeader(buffer, MSGPACK, payloadLen);

    // Copy payload
    memcpy(buffer + HEADER_SIZE, payloadBuf, payloadLen);
    free(payloadBuf);

    log_d("Encoded MessagePack: %d bytes (payload: %d)", *outLen, payloadLen);
    return buffer;
    #else
    (void)doc;
    log_e("MessagePack support not compiled in");
    *outLen = 0;
    return NULL;
    #endif
}

//=============================================================================
// Helper Functions
//=============================================================================

static void buildHeader(uint8_t *header, MessageFormat format, uint16_t payloadLen)
{
    header[0] = (uint8_t)(format);
    header[1] = payloadLen & 0xFF;         // Low byte
    header[2] = (payloadLen >> 8) & 0xFF;  // High byte
    header[3] = 0x00;                      // Reserved
}

static bool parseHeader(const uint8_t *header, MessageFormat *format, uint16_t *payloadLen)
{
    if (header == NULL) {
        return false;
    }

    uint8_t magic = header[0];

    // Validate magic byte
    if (magic != JSON &&
        magic != MSGPACK &&
        magic != BINARY) {
        return false;
    }

    *format = (MessageFormat)(magic);
    *payloadLen = header[1] | (header[2] << 8);  // Little-endian

    return true;
}

//=============================================================================
// MessagePack <-> cJSON Conversion (if mpack available)
//=============================================================================

#ifdef HAS_MPACK

static cJSON *mpackNodeToJson(mpack_node_t node)
{
    mpack_type_t type = mpack_node_type(node);

    switch (type) {
        case mpack_type_nil:
            return cJSON_CreateNull();

        case mpack_type_bool:
            return cJSON_CreateBool(mpack_node_bool(node));

        case mpack_type_int:
            return cJSON_CreateNumber(mpack_node_i64(node));

        case mpack_type_uint:
            return cJSON_CreateNumber(mpack_node_u64(node));

        case mpack_type_float:
            return cJSON_CreateNumber(mpack_node_float(node));

        case mpack_type_double:
            return cJSON_CreateNumber(mpack_node_double(node));

        case mpack_type_str: {
            size_t len = mpack_node_strlen(node);
            const char *str = mpack_node_str(node);
            char *nullTermStr = (char *)malloc(len + 1);
            if (nullTermStr) {
                memcpy(nullTermStr, str, len);
                nullTermStr[len] = '\0';
                cJSON *item = cJSON_CreateString(nullTermStr);
                free(nullTermStr);
                return item;
            }
            return NULL;
        }

        case mpack_type_array:
            return mpackArrayToJson(node);

        case mpack_type_map:
            return mpackMapToJson(node);

        default:
            log_w("Unsupported mpack type: %d", type);
            return cJSON_CreateNull();
    }
}

static cJSON *mpackArrayToJson(mpack_node_t node)
{
    cJSON *array = cJSON_CreateArray();
    if (array == NULL) {
        return NULL;
    }

    size_t count = mpack_node_array_length(node);
    for (size_t i = 0; i < count; i++) {
        mpack_node_t elem = mpack_node_array_at(node, i);
        cJSON *item = mpackNodeToJson(elem);
        if (item) {
            cJSON_AddItemToArray(array, item);
        }
    }

    return array;
}

static cJSON *mpackMapToJson(mpack_node_t node)
{
    cJSON *object = cJSON_CreateObject();
    if (object == NULL) {
        return NULL;
    }

    size_t count = mpack_node_map_count(node);
    for (size_t i = 0; i < count; i++) {
        mpack_node_t keyNode = mpack_node_map_key_at(node, i);
        mpack_node_t valueNode = mpack_node_map_value_at(node, i);

        // Get key as string
        size_t keyLen = mpack_node_strlen(keyNode);
        const char *keyStr = mpack_node_str(keyNode);
        char *key = (char *)malloc(keyLen + 1);
        if (key) {
            memcpy(key, keyStr, keyLen);
            key[keyLen] = '\0';

            // Convert value
            cJSON *value = mpackNodeToJson(valueNode);
            if (value) {
                cJSON_AddItemToObject(object, key, value);
            }

            free(key);
        }
    }

    return object;
}

static void jsonToMpack(mpack_writer_t *writer, cJSON *item)
{
    if (item == NULL) {
        mpack_write_nil(writer);
        return;
    }

    switch (item->type & 0xFF) {
        case cJSON_NULL:
            mpack_write_nil(writer);
            break;

        case cJSON_False:
            mpack_write_bool(writer, false);
            break;

        case cJSON_True:
            mpack_write_bool(writer, true);
            break;

        case cJSON_Number:
            // Check if integer or float
            if (item->valuedouble == (double)item->valueint) {
                mpack_write_int(writer, item->valueint);
            } else {
                mpack_write_double(writer, item->valuedouble);
            }
            break;

        case cJSON_String:
            if (item->valuestring) {
                mpack_write_str(writer, item->valuestring, strlen(item->valuestring));
            } else {
                mpack_write_nil(writer);
            }
            break;

        case cJSON_Array:
            jsonArrayToMpack(writer, item);
            break;

        case cJSON_Object:
            jsonObjectToMpack(writer, item);
            break;

        default:
            log_w("Unknown cJSON type: %d", item->type);
            mpack_write_nil(writer);
            break;
    }
}

static void jsonArrayToMpack(mpack_writer_t *writer, cJSON *array)
{
    // Count elements
    int count = cJSON_GetArraySize(array);
    mpack_start_array(writer, count);

    // Write elements
    cJSON *item = array->child;
    while (item != NULL) {
        jsonToMpack(writer, item);
        item = item->next;
    }

    mpack_finish_array(writer);
}

static void jsonObjectToMpack(mpack_writer_t *writer, cJSON *object)
{
    // Count properties
    int count = 0;
    cJSON *item = object->child;
    while (item != NULL) {
        count++;
        item = item->next;
    }

    mpack_start_map(writer, count);

    // Write key-value pairs
    item = object->child;
    while (item != NULL) {
        if (item->string) {
            mpack_write_str(writer, item->string, strlen(item->string));
            jsonToMpack(writer, item);
        }
        item = item->next;
    }

    mpack_finish_map(writer);
}

#endif // HAS_MPACK
