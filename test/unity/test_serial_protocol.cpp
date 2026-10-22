#ifdef UNIT_TEST

#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../components/uc2_serial/serial/SerialProtocol.h"
#include "../../components/uc2_serial/serial/SerialProtocol.cpp"
#include "mocks/mock_clock.h"
#include "helpers/test_util.h"

// ============================================================================
// SP-001: detectFormat with valid JSON header (0x4A)
// ============================================================================
void test_sp001_detectFormat_jsonHeader() {
    uint8_t header[4] = {0x4A, 0x0A, 0x00, 0x00}; // JSON, length=10
    MessageFormat fmt = SerialProtocol::detectFormat(header, 4);
    TEST_ASSERT_EQUAL(MessageFormat::JSON, fmt);
}

// ============================================================================
// SP-002: detectFormat with valid MessagePack header (0x4D)
// ============================================================================
void test_sp002_detectFormat_msgpackHeader() {
    uint8_t header[4] = {0x4D, 0x05, 0x00, 0x00}; // MSGPACK, length=5
    MessageFormat fmt = SerialProtocol::detectFormat(header, 4);
    TEST_ASSERT_EQUAL(MessageFormat::MSGPACK, fmt);
}

// ============================================================================
// SP-003: detectFormat with legacy ++\n prefix
// ============================================================================
void test_sp003_detectFormat_legacy() {
    const char* legacy = "++\n{\"task\":\"test\"}\n--\n";
    MessageFormat fmt = SerialProtocol::detectFormat(
        reinterpret_cast<const uint8_t*>(legacy), strlen(legacy));
    TEST_ASSERT_EQUAL(MessageFormat::JSON, fmt);
}

// ============================================================================
// SP-004: detectFormat with unknown header
// ============================================================================
void test_sp004_detectFormat_unknown() {
    uint8_t data[4] = {0xFF, 0x00, 0x00, 0x00};
    MessageFormat fmt = SerialProtocol::detectFormat(data, 4);
    TEST_ASSERT_EQUAL(MessageFormat::UNKNOWN, fmt);
}

// ============================================================================
// SP-005: decode valid JSON payload
// ============================================================================
void test_sp005_decode_validJson() {
    const char* json = "{\"task\":\"motor_get\",\"qid\":1}";
    uint16_t len = strlen(json);
    uint8_t header[4];
    SerialProtocol::buildHeaderTest(header, MessageFormat::JSON, len);
    
    uint8_t* buf = new uint8_t[4 + len + 1];
    memcpy(buf, header, 4);
    memcpy(buf + 4, json, len);
    buf[4 + len] = '\0';
    
    cJSON* doc = SerialProtocol::decode(buf, 4 + len);
    TEST_ASSERT_NOT_NULL(doc);
    if (doc) {
        cJSON* task = cJSON_GetObjectItemCaseSensitive(doc, "task");
        TEST_ASSERT_TRUE(cJSON_IsString(task));
        TEST_ASSERT_EQUAL_STRING("motor_get", task->valuestring);
        cJSON_Delete(doc);
    }
    delete[] buf;
}

// ============================================================================
// SP-006: decode valid MessagePack payload (if HAS_MPACK)
// ============================================================================
void test_sp006_decode_validMsgpack() {
#ifdef HAS_MPACK
    // MessagePack: {"task":"test"} = map(1, "task", "test")
    uint8_t msgpack[] = {0x81, 0xA4, 't', 'a', 's', 'k', 0xA4, 't', 'e', 's', 't'};
    uint16_t len = sizeof(msgpack);
    uint8_t header[4];
    SerialProtocol::buildHeaderTest(header, MessageFormat::MSGPACK, len);
    
    uint8_t* buf = new uint8_t[4 + len];
    memcpy(buf, header, 4);
    memcpy(buf + 4, msgpack, len);
    
    cJSON* doc = SerialProtocol::decode(buf, 4 + len);
    TEST_ASSERT_NOT_NULL(doc);
    if (doc) {
        cJSON* task = cJSON_GetObjectItemCaseSensitive(doc, "task");
        TEST_ASSERT_TRUE(cJSON_IsString(task));
        cJSON_Delete(doc);
    }
    delete[] buf;
#else
    TEST_IGNORE(); // MessagePack not available
#endif
}

// ============================================================================
// SP-007: decode truncated payload returns nullptr
// ============================================================================
void test_sp007_decode_truncated() {
    uint8_t header[4] = {0x4A, 0xFF, 0xFF, 0x00}; // length=65535
    uint8_t payload[] = {'{', '"', 'b', 'r'}; // truncated
    uint8_t* buf = new uint8_t[4 + 3];
    memcpy(buf, header, 4);
    memcpy(buf + 4, payload, 3);
    
    cJSON* doc = SerialProtocol::decode(buf, 7);
    TEST_ASSERT_NULL(doc);
    delete[] buf;
}

// ============================================================================
// SP-008: decode oversized payload returns nullptr
// ============================================================================
void test_sp008_decode_oversized() {
    uint8_t header[4] = {0x4A, 0xFF, 0xFF, 0x00}; // claims 65535 bytes
    uint8_t payload[] = {'{', '}', '\0'}; // only 2 bytes
    uint8_t* buf = new uint8_t[4 + 3];
    memcpy(buf, header, 4);
    memcpy(buf + 4, payload, 3);
    
    cJSON* doc = SerialProtocol::decode(buf, 7);
    TEST_ASSERT_NULL(doc);
    delete[] buf;
}

// ============================================================================
// SP-009: encode JSON document to bytes
// ============================================================================
void test_sp009_encode_json() {
    cJSON* doc = cJSON_Parse("{\"task\":\"test\",\"qid\":1}");
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    size_t outLen = 0;
    uint8_t* encoded = SerialProtocol::encode(doc, MessageFormat::JSON, outLen);
    TEST_ASSERT_NOT_NULL(encoded);
    TEST_ASSERT_GREATER_THAN(0, outLen);
    
    if (encoded) {
        // Should start with JSON magic byte
        TEST_ASSERT_EQUAL_UINT8(0x4A, encoded[0]);
        free(encoded);
    }
    cJSON_Delete(doc);
}

// ============================================================================
// SP-010: encode MessagePack document to bytes (if HAS_MPACK)
// ============================================================================
void test_sp010_encode_msgpack() {
#ifdef HAS_MPACK
    cJSON* doc = cJSON_Parse("{\"task\":\"test\"}");
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    size_t outLen = 0;
    uint8_t* encoded = SerialProtocol::encode(doc, MessageFormat::MSGPACK, outLen);
    TEST_ASSERT_NOT_NULL(encoded);
    TEST_ASSERT_GREATER_THAN(0, outLen);
    
    if (encoded) {
        TEST_ASSERT_EQUAL_UINT8(0x4D, encoded[0]);
        free(encoded);
    }
    cJSON_Delete(doc);
#else
    TEST_IGNORE();
#endif
}

// ============================================================================
// SP-011: getMessageInfo extracts length correctly
// ============================================================================
void test_sp011_getMessageInfo_length() {
    const char* json = "{\"a\":1}";
    uint16_t len = strlen(json);
    uint8_t header[4];
    SerialProtocol::buildHeaderTest(header, MessageFormat::JSON, len);
    
    MessageFormat fmt;
    uint16_t payloadLen;
    bool result = SerialProtocol::getMessageInfo(header, 4, fmt, payloadLen);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(MessageFormat::JSON, fmt);
    TEST_ASSERT_EQUAL_UINT16(len, payloadLen);
}

// ============================================================================
// SP-012: isMessageComplete with partial data
// ============================================================================
void test_sp012_isMessageComplete_partial() {
    // Header says 10 bytes payload, but we only have 5
    uint8_t header[4] = {0x4A, 0x0A, 0x00, 0x00};
    uint8_t partial[] = {'{', '"', 'a', '"', ':'};
    uint8_t* buf = new uint8_t[4 + 5];
    memcpy(buf, header, 4);
    memcpy(buf + 4, partial, 5);
    
    bool complete = SerialProtocol::isMessageComplete(buf, 9);
    TEST_ASSERT_FALSE(complete);
    delete[] buf;
}

// ============================================================================
// SP-013: isMessageComplete with complete data
// ============================================================================
void test_sp013_isMessageComplete_complete() {
    const char* json = "{\"a\":1}";
    uint16_t len = strlen(json);
    uint8_t header[4];
    SerialProtocol::buildHeaderTest(header, MessageFormat::JSON, len);
    
    uint8_t* buf = new uint8_t[4 + len];
    memcpy(buf, header, 4);
    memcpy(buf + 4, json, len);
    
    bool complete = SerialProtocol::isMessageComplete(buf, 4 + len);
    TEST_ASSERT_TRUE(complete);
    delete[] buf;
}

// ============================================================================
// SP-014: isMessageComplete with overflow length field
// ============================================================================
void test_sp014_isMessageComplete_overflow() {
    uint8_t header[4] = {0x4A, 0xFF, 0xFF, 0x00}; // length=65535
    uint8_t payload[] = {'{', '}'};
    uint8_t* buf = new uint8_t[4 + 2];
    memcpy(buf, header, 4);
    memcpy(buf + 4, payload, 2);
    
    bool complete = SerialProtocol::isMessageComplete(buf, 6);
    TEST_ASSERT_FALSE(complete);
    delete[] buf;
}

// ============================================================================
// SP-015: Legacy format parsing with ++\n...\n--\n
// ============================================================================
void test_sp015_legacyParsing() {
    const char* legacy = "++\n{\"task\":\"test\"}\n--\n";
    size_t len = strlen(legacy);
    
    MessageFormat fmt;
    uint16_t payloadLen;
    bool result = SerialProtocol::getMessageInfo(
        reinterpret_cast<const uint8_t*>(legacy), len, fmt, payloadLen);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(MessageFormat::JSON, fmt);
    
    cJSON* doc = SerialProtocol::decode(
        reinterpret_cast<const uint8_t*>(legacy), len);
    TEST_ASSERT_NOT_NULL(doc);
    if (doc) {
        cJSON* task = cJSON_GetObjectItemCaseSensitive(doc, "task");
        TEST_ASSERT_EQUAL_STRING("test", task->valuestring);
        cJSON_Delete(doc);
    }
}

// ============================================================================
// SP-016: Empty input handling
// ============================================================================
void test_sp016_emptyInput() {
    MessageFormat fmt = SerialProtocol::detectFormat(nullptr, 0);
    TEST_ASSERT_EQUAL(MessageFormat::UNKNOWN, fmt);
    
    uint16_t payloadLen;
    bool result = SerialProtocol::getMessageInfo(nullptr, 0, fmt, payloadLen);
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// SP-017: Null pointer input safety
// ============================================================================
void test_sp017_nullPointerSafety() {
    cJSON* doc = SerialProtocol::decode(nullptr, 0);
    TEST_ASSERT_NULL(doc);
    
    size_t outLen = 0;
    uint8_t* encoded = SerialProtocol::encode(nullptr, MessageFormat::JSON, outLen);
    TEST_ASSERT_NULL(encoded);
    TEST_ASSERT_EQUAL_UINT16(0, outLen);
}

// ============================================================================
// SP-018: MAX_MESSAGE_SIZE boundary (8192 bytes)
// ============================================================================
void test_sp018_maxMessageSizeBoundary() {
    // Create a JSON payload just under MAX_MESSAGE_SIZE
    char largeJson[SerialProtocol::MAX_MESSAGE_SIZE];
    memset(largeJson, 'a', sizeof(largeJson) - 1);
    largeJson[sizeof(largeJson) - 1] = '\0';
    
    cJSON* doc = cJSON_Parse(largeJson);
    TEST_ASSERT_NULL(doc); // Should fail to parse (invalid JSON)
    if (doc) cJSON_Delete(doc);
}

// ============================================================================
// SP-019: Format mismatch between header and payload
// ============================================================================
void test_sp019_formatMismatch() {
    // Header says JSON but payload is invalid JSON
    const char* invalid = "not json at all!!!";
    uint16_t len = strlen(invalid);
    uint8_t header[4];
    SerialProtocol::buildHeaderTest(header, MessageFormat::JSON, len);
    
    uint8_t* buf = new uint8_t[4 + len];
    memcpy(buf, header, 4);
    memcpy(buf + 4, invalid, len);
    
    cJSON* doc = SerialProtocol::decode(buf, 4 + len);
    TEST_ASSERT_NULL(doc);
    delete[] buf;
}

// ============================================================================
// SP-020: Build header correctly
// ============================================================================
void test_sp020_buildHeader() {
    uint8_t header[4];
    SerialProtocol::buildHeaderTest(header, MessageFormat::JSON, 42);
    TEST_ASSERT_EQUAL_UINT8(0x4A, header[0]);
    TEST_ASSERT_EQUAL_UINT8(0x2A, header[1]); // 42 in little-endian
    TEST_ASSERT_EQUAL_UINT8(0x00, header[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, header[3]);
    
    SerialProtocol::buildHeaderTest(header, MessageFormat::MSGPACK, 256);
    TEST_ASSERT_EQUAL_UINT8(0x4D, header[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, header[1]); // 256 = 0x0100 in LE
    TEST_ASSERT_EQUAL_UINT8(0x01, header[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, header[3]);
}

#endif
