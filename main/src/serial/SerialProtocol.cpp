#include "SerialProtocol.h"
#include <string.h>

// Check if mpack is available
#ifdef HAS_MPACK
#include <mpack.h>
#endif

// Define static constants
const char* SerialProtocol::LEGACY_START = "++\n";
const char* SerialProtocol::LEGACY_END = "\n--\n";

//=============================================================================
// Public Interface
//=============================================================================

MessageFormat SerialProtocol::detectFormat(const uint8_t* data, size_t len)
{
    if (data == nullptr || len < 1) {
        return MessageFormat::UNKNOWN;
    }
    
    // Check for legacy format first
    if (len >= LEGACY_START_LEN && 
        memcmp(data, LEGACY_START, LEGACY_START_LEN) == 0) {
        return MessageFormat::JSON;  // Legacy is still JSON
    }
    
    // Check magic byte for new format
    uint8_t magic = data[0];
    
    switch (magic) {
        case static_cast<uint8_t>(MessageFormat::JSON):
            return MessageFormat::JSON;
        case static_cast<uint8_t>(MessageFormat::MSGPACK):
            return MessageFormat::MSGPACK;
        case static_cast<uint8_t>(MessageFormat::BINARY):
            return MessageFormat::BINARY;
        default:
            return MessageFormat::UNKNOWN;
    }
}

bool SerialProtocol::getMessageInfo(const uint8_t* data, size_t len,
                                   MessageFormat& format, uint16_t& payloadLen)
{
    format = detectFormat(data, len);
    payloadLen = 0;
    
    if (format == MessageFormat::UNKNOWN) {
        return false;
    }
    
    // Legacy format - find end delimiter
    if (len >= LEGACY_START_LEN && 
        memcmp(data, LEGACY_START, LEGACY_START_LEN) == 0) {
        // Search for end delimiter
        for (size_t i = LEGACY_START_LEN; i < len - LEGACY_END_LEN; i++) {
            if (memcmp(data + i, LEGACY_END, LEGACY_END_LEN) == 0) {
                payloadLen = i - LEGACY_START_LEN;
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

bool SerialProtocol::isMessageComplete(const uint8_t* data, size_t len)
{
    MessageFormat format;
    uint16_t payloadLen;
    
    if (!getMessageInfo(data, len, format, payloadLen)) {
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

cJSON* SerialProtocol::decode(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0) {
        log_e("Invalid input to decode");
        return nullptr;
    }
    
    MessageFormat format = detectFormat(data, len);
    
    switch (format) {
        case MessageFormat::MSGPACK:
            return decodeMsgPack(data + HEADER_SIZE, 
                               *reinterpret_cast<const uint16_t*>(data + 1));
        
        case MessageFormat::JSON:
            // Check if legacy or new format
            if (memcmp(data, LEGACY_START, LEGACY_START_LEN) == 0) {
                return decodeLegacy(data, len);
            } else {
                return decodeJson(data + HEADER_SIZE,
                                *reinterpret_cast<const uint16_t*>(data + 1));
            }
        
        default:
            log_e("Unknown message format: 0x%02X", data[0]);
            // Try legacy format as fallback
            return decodeLegacy(data, len);
    }
}

uint8_t* SerialProtocol::encode(cJSON* doc, MessageFormat format, size_t& outLen)
{
    if (doc == nullptr) {
        log_e("Cannot encode NULL cJSON document");
        outLen = 0;
        return nullptr;
    }
    
    switch (format) {
        case MessageFormat::MSGPACK:
            #ifdef HAS_MPACK
            return encodeMsgPack(doc, outLen);
            #else
            log_w("MessagePack not available, falling back to JSON");
            return encodeJson(doc, outLen);
            #endif
        
        case MessageFormat::JSON:
            return encodeJson(doc, outLen);
        
        default:
            log_e("Unsupported encoding format: %d", static_cast<int>(format));
            outLen = 0;
            return nullptr;
    }
}

//=============================================================================
// Private Decoders
//=============================================================================

cJSON* SerialProtocol::decodeJson(const uint8_t* payload, size_t len)
{
    if (payload == nullptr || len == 0 || len > MAX_MESSAGE_SIZE) {
        log_e("Invalid JSON payload: len=%d", len);
        return nullptr;
    }
    
    // Ensure null termination for cJSON_Parse
    char* jsonStr = (char*)malloc(len + 1);
    if (jsonStr == nullptr) {
        log_e("Failed to allocate memory for JSON string");
        return nullptr;
    }
    
    memcpy(jsonStr, payload, len);
    jsonStr[len] = '\0';
    
    cJSON* doc = cJSON_Parse(jsonStr);
    free(jsonStr);
    
    if (doc == nullptr) {
        log_e("Failed to parse JSON");
        return nullptr;
    }
    
    log_d("Decoded JSON: %d bytes", len);
    return doc;
}

cJSON* SerialProtocol::decodeLegacy(const uint8_t* data, size_t len)
{
    if (data == nullptr || len < LEGACY_START_LEN + LEGACY_END_LEN) {
        return nullptr;
    }
    
    // Find start delimiter
    const uint8_t* start = data;
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
            return nullptr;
        }
    }
    
    // Find end delimiter
    const uint8_t* end = nullptr;
    size_t remaining = len - (start - data) - LEGACY_START_LEN;
    for (size_t i = 0; i < remaining - LEGACY_END_LEN; i++) {
        if (memcmp(start + LEGACY_START_LEN + i, LEGACY_END, LEGACY_END_LEN) == 0) {
            end = start + LEGACY_START_LEN + i;
            break;
        }
    }
    
    if (end == nullptr) {
        log_e("Legacy format: end delimiter not found");
        return nullptr;
    }
    
    // Extract JSON payload
    size_t jsonLen = end - (start + LEGACY_START_LEN);
    const uint8_t* jsonStart = start + LEGACY_START_LEN;
    
    log_d("Decoded legacy JSON: %d bytes", jsonLen);
    return decodeJson(jsonStart, jsonLen);
}

cJSON* SerialProtocol::decodeMsgPack(const uint8_t* payload, size_t len)
{
    #ifdef HAS_MPACK
    if (payload == nullptr || len == 0 || len > MAX_MESSAGE_SIZE) {
        log_e("Invalid MessagePack payload: len=%d", len);
        return nullptr;
    }
    
    // Parse MessagePack data
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, (const char*)payload, len);
    mpack_tree_parse(&tree);
    
    if (mpack_tree_error(&tree) != mpack_ok) {
        log_e("MessagePack parse error: %d", mpack_tree_error(&tree));
        mpack_tree_destroy(&tree);
        return nullptr;
    }
    
    // Convert to cJSON
    mpack_node_t root = mpack_tree_root(&tree);
    cJSON* doc = mpackNodeToJson(root);
    
    mpack_tree_destroy(&tree);
    
    if (doc == nullptr) {
        log_e("Failed to convert MessagePack to cJSON");
        return nullptr;
    }
    
    log_d("Decoded MessagePack: %d bytes", len);
    return doc;
    #else
    log_e("MessagePack support not compiled in");
    return nullptr;
    #endif
}

//=============================================================================
// Private Encoders
//=============================================================================

uint8_t* SerialProtocol::encodeJson(cJSON* doc, size_t& outLen)
{
    // Serialize cJSON to string
    char* jsonStr = cJSON_PrintUnformatted(doc);
    if (jsonStr == nullptr) {
        log_e("Failed to serialize cJSON to string");
        outLen = 0;
        return nullptr;
    }
    
    size_t payloadLen = strlen(jsonStr);
    
    if (payloadLen > MAX_MESSAGE_SIZE) {
        log_e("JSON payload too large: %d > %d", payloadLen, MAX_MESSAGE_SIZE);
        free(jsonStr);
        outLen = 0;
        return nullptr;
    }
    
    // Allocate buffer for header + payload
    outLen = HEADER_SIZE + payloadLen;
    uint8_t* buffer = (uint8_t*)malloc(outLen);
    if (buffer == nullptr) {
        log_e("Failed to allocate encoding buffer");
        free(jsonStr);
        outLen = 0;
        return nullptr;
    }
    
    // Build header
    buildHeader(buffer, MessageFormat::JSON, payloadLen);
    
    // Copy payload
    memcpy(buffer + HEADER_SIZE, jsonStr, payloadLen);
    free(jsonStr);
    
    log_d("Encoded JSON: %d bytes (payload: %d)", outLen, payloadLen);
    return buffer;
}

uint8_t* SerialProtocol::encodeLegacy(cJSON* doc, size_t& outLen)
{
    // Serialize cJSON to string
    char* jsonStr = cJSON_PrintUnformatted(doc);
    if (jsonStr == nullptr) {
        log_e("Failed to serialize cJSON to string");
        outLen = 0;
        return nullptr;
    }
    
    size_t payloadLen = strlen(jsonStr);
    outLen = LEGACY_START_LEN + payloadLen + LEGACY_END_LEN;
    
    uint8_t* buffer = (uint8_t*)malloc(outLen);
    if (buffer == nullptr) {
        log_e("Failed to allocate encoding buffer");
        free(jsonStr);
        outLen = 0;
        return nullptr;
    }
    
    // Build message: "++\n<JSON>\n--\n"
    memcpy(buffer, LEGACY_START, LEGACY_START_LEN);
    memcpy(buffer + LEGACY_START_LEN, jsonStr, payloadLen);
    memcpy(buffer + LEGACY_START_LEN + payloadLen, LEGACY_END, LEGACY_END_LEN);
    
    free(jsonStr);
    
    log_d("Encoded legacy JSON: %d bytes", outLen);
    return buffer;
}

uint8_t* SerialProtocol::encodeMsgPack(cJSON* doc, size_t& outLen)
{
    #ifdef HAS_MPACK
    // Estimate buffer size (conservative: 2x JSON size)
    char* jsonStr = cJSON_PrintUnformatted(doc);
    size_t estimatedSize = jsonStr ? strlen(jsonStr) * 2 : 1024;
    free(jsonStr);
    
    if (estimatedSize > MAX_MESSAGE_SIZE) {
        estimatedSize = MAX_MESSAGE_SIZE;
    }
    
    // Allocate payload buffer
    char* payloadBuf = (char*)malloc(estimatedSize);
    if (payloadBuf == nullptr) {
        log_e("Failed to allocate MessagePack buffer");
        outLen = 0;
        return nullptr;
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
        outLen = 0;
        return nullptr;
    }
    
    size_t payloadLen = mpack_writer_buffer_used(&writer);
    
    // Allocate final buffer with header
    outLen = HEADER_SIZE + payloadLen;
    uint8_t* buffer = (uint8_t*)malloc(outLen);
    if (buffer == nullptr) {
        log_e("Failed to allocate final buffer");
        free(payloadBuf);
        outLen = 0;
        return nullptr;
    }
    
    // Build header
    buildHeader(buffer, MessageFormat::MSGPACK, payloadLen);
    
    // Copy payload
    memcpy(buffer + HEADER_SIZE, payloadBuf, payloadLen);
    free(payloadBuf);
    
    log_d("Encoded MessagePack: %d bytes (payload: %d)", outLen, payloadLen);
    return buffer;
    #else
    log_e("MessagePack support not compiled in");
    outLen = 0;
    return nullptr;
    #endif
}

//=============================================================================
// Helper Functions
//=============================================================================

void SerialProtocol::buildHeader(uint8_t* header, MessageFormat format, uint16_t payloadLen)
{
    header[0] = static_cast<uint8_t>(format);
    header[1] = payloadLen & 0xFF;         // Low byte
    header[2] = (payloadLen >> 8) & 0xFF;  // High byte
    header[3] = 0x00;                      // Reserved
}

bool SerialProtocol::parseHeader(const uint8_t* header, MessageFormat& format, uint16_t& payloadLen)
{
    if (header == nullptr) {
        return false;
    }
    
    uint8_t magic = header[0];
    
    // Validate magic byte
    if (magic != static_cast<uint8_t>(MessageFormat::JSON) &&
        magic != static_cast<uint8_t>(MessageFormat::MSGPACK) &&
        magic != static_cast<uint8_t>(MessageFormat::BINARY)) {
        return false;
    }
    
    format = static_cast<MessageFormat>(magic);
    payloadLen = header[1] | (header[2] << 8);  // Little-endian
    
    return true;
}

//=============================================================================
// MessagePack <-> cJSON Conversion (if mpack available)
//=============================================================================

#ifdef HAS_MPACK

cJSON* SerialProtocol::mpackNodeToJson(mpack_node_t node)
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
            const char* str = mpack_node_str(node);
            char* nullTermStr = (char*)malloc(len + 1);
            if (nullTermStr) {
                memcpy(nullTermStr, str, len);
                nullTermStr[len] = '\0';
                cJSON* item = cJSON_CreateString(nullTermStr);
                free(nullTermStr);
                return item;
            }
            return nullptr;
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

cJSON* SerialProtocol::mpackArrayToJson(mpack_node_t node)
{
    cJSON* array = cJSON_CreateArray();
    if (array == nullptr) {
        return nullptr;
    }
    
    size_t count = mpack_node_array_length(node);
    for (size_t i = 0; i < count; i++) {
        mpack_node_t elem = mpack_node_array_at(node, i);
        cJSON* item = mpackNodeToJson(elem);
        if (item) {
            cJSON_AddItemToArray(array, item);
        }
    }
    
    return array;
}

cJSON* SerialProtocol::mpackMapToJson(mpack_node_t node)
{
    cJSON* object = cJSON_CreateObject();
    if (object == nullptr) {
        return nullptr;
    }
    
    size_t count = mpack_node_map_count(node);
    for (size_t i = 0; i < count; i++) {
        mpack_node_t keyNode = mpack_node_map_key_at(node, i);
        mpack_node_t valueNode = mpack_node_map_value_at(node, i);
        
        // Get key as string
        size_t keyLen = mpack_node_strlen(keyNode);
        const char* keyStr = mpack_node_str(keyNode);
        char* key = (char*)malloc(keyLen + 1);
        if (key) {
            memcpy(key, keyStr, keyLen);
            key[keyLen] = '\0';
            
            // Convert value
            cJSON* value = mpackNodeToJson(valueNode);
            if (value) {
                cJSON_AddItemToObject(object, key, value);
            }
            
            free(key);
        }
    }
    
    return object;
}

void SerialProtocol::jsonToMpack(mpack_writer_t* writer, cJSON* item)
{
    if (item == nullptr) {
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

void SerialProtocol::jsonArrayToMpack(mpack_writer_t* writer, cJSON* array)
{
    // Count elements
    int count = cJSON_GetArraySize(array);
    mpack_start_array(writer, count);
    
    // Write elements
    cJSON* item = array->child;
    while (item != nullptr) {
        jsonToMpack(writer, item);
        item = item->next;
    }
    
    mpack_finish_array(writer);
}

void SerialProtocol::jsonObjectToMpack(mpack_writer_t* writer, cJSON* object)
{
    // Count properties
    int count = 0;
    cJSON* item = object->child;
    while (item != nullptr) {
        count++;
        item = item->next;
    }
    
    mpack_start_map(writer, count);
    
    // Write key-value pairs
    item = object->child;
    while (item != nullptr) {
        if (item->string) {
            mpack_write_str(writer, item->string, strlen(item->string));
            jsonToMpack(writer, item);
        }
        item = item->next;
    }
    
    mpack_finish_map(writer);
}

#endif // HAS_MPACK
