# ESP32 MessagePack Integration Guide

## Overview

This guide explains how to add MessagePack support to the ESP32 firmware for the UC2 project.

## Required Library

Use the mpack library for ESP32 MessagePack support:

```ini
lib_deps = 
    ...existing dependencies...
    ludocode/mpack@^1.1.1
```

## Integration Steps

### 1. Add Library Dependency

Update `platformio.ini` to include mpack:

```ini
[env:esp32_framework]
lib_deps = 
    teemuatlut/TMCStepper@^0.7.3
    madhephaestus/ESP32Encoder@^0.12.0
    ludocode/mpack@^1.1.1  # Add this line
```

### 2. Create Protocol Handler

Create `main/src/serial/SerialProtocol.h` and `main/src/serial/SerialProtocol.cpp`:

**SerialProtocol.h:**
```cpp
#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>
#include <cJSON.h>

enum class MessageFormat : uint8_t {
    JSON = 0x4A,      // 'J'
    MSGPACK = 0x4D,   // 'M'
    BINARY = 0x42,    // 'B' (reserved)
    UNKNOWN = 0x00
};

class SerialProtocol {
public:
    static const uint8_t HEADER_SIZE = 4;
    static const uint16_t MAX_MESSAGE_SIZE = 8192;
    
    // Detect message format from first byte
    static MessageFormat detectFormat(const uint8_t* data, size_t len);
    
    // Decode received message to cJSON
    static cJSON* decode(const uint8_t* data, size_t len);
    
    // Encode cJSON to bytes for transmission
    static uint8_t* encode(cJSON* doc, MessageFormat format, size_t& outLen);
    
    // Get message info without full parsing
    static bool getMessageInfo(const uint8_t* data, size_t len, 
                              MessageFormat& format, uint16_t& payloadLen);
    
private:
    static cJSON* decodeMsgPack(const uint8_t* payload, size_t len);
    static cJSON* decodeJson(const uint8_t* payload, size_t len);
    static cJSON* decodeLegacy(const uint8_t* data, size_t len);
    
    static uint8_t* encodeMsgPack(cJSON* doc, size_t& outLen);
    static uint8_t* encodeJson(cJSON* doc, size_t& outLen);
};

#endif // SERIAL_PROTOCOL_H
```

**SerialProtocol.cpp:**
See implementation file below.

### 3. Update SerialProcess.cpp

Modify the serial input handling to use the new protocol:

**Before:**
```cpp
// Old JSON-only parsing
cJSON *root = cJSON_Parse(buffer);
```

**After:**
```cpp
// New multi-format parsing
#include "SerialProtocol.h"

MessageFormat format = SerialProtocol::detectFormat((uint8_t*)buffer, len);
cJSON *root = SerialProtocol::decode((uint8_t*)buffer, len);
```

### 4. Update Output Functions

Modify `serialize()` to support MessagePack:

```cpp
void serialize(cJSON *doc)
{
    if (doc == NULL) return;
    
    // Detect if client supports MessagePack (track during handshake)
    MessageFormat format = clientSupportsMsgPack ? 
                          MessageFormat::MSGPACK : MessageFormat::JSON;
    
    size_t encodedLen = 0;
    uint8_t* encoded = SerialProtocol::encode(doc, format, encodedLen);
    
    if (encoded != nullptr) {
        Serial.write(encoded, encodedLen);
        free(encoded);
    }
    
    if (doc != NULL) {
        cJSON_Delete(doc);
    }
}
```

### 5. Protocol Negotiation

Add capability detection on startup:

```cpp
// In setup() or during first communication:
// Client sends: {"task": "/capabilities_get", "qid": 1}
// ESP32 responds: {"qid": 1, "msgpack": true, "version": "2.0"}

bool clientSupportsMsgPack = false;  // Global flag

void handleCapabilitiesGet(cJSON *root) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddNumberToObject(response, "qid", getQid(root));
    cJSON_AddBoolToObject(response, "msgpack", true);
    cJSON_AddStringToObject(response, "protocol_version", "2.0");
    serialize(response);
}
```

## Backward Compatibility

The implementation maintains full backward compatibility:

1. **Legacy JSON format** (`++\n...\n--\n`) is still supported
2. **Auto-detection** determines format from first bytes
3. **Fallback to JSON** if MessagePack parsing fails
4. **Client-driven** protocol selection (client requests MessagePack explicitly)

## Testing

### Unit Tests

Create `test/test_serial_protocol.cpp`:

```cpp
#include <unity.h>
#include "serial/SerialProtocol.h"

void test_detect_msgpack() {
    uint8_t data[] = {0x4D, 0x10, 0x00, 0x00, /* msgpack data */};
    MessageFormat fmt = SerialProtocol::detectFormat(data, sizeof(data));
    TEST_ASSERT_EQUAL(MessageFormat::MSGPACK, fmt);
}

void test_detect_json() {
    uint8_t data[] = {0x4A, 0x10, 0x00, 0x00, /* json data */};
    MessageFormat fmt = SerialProtocol::detectFormat(data, sizeof(data));
    TEST_ASSERT_EQUAL(MessageFormat::JSON, fmt);
}

void test_roundtrip_msgpack() {
    cJSON *original = cJSON_CreateObject();
    cJSON_AddStringToObject(original, "task", "/state_get");
    cJSON_AddNumberToObject(original, "qid", 42);
    
    size_t encodedLen;
    uint8_t* encoded = SerialProtocol::encode(original, 
                                             MessageFormat::MSGPACK, 
                                             encodedLen);
    TEST_ASSERT_NOT_NULL(encoded);
    
    cJSON *decoded = SerialProtocol::decode(encoded, encodedLen);
    TEST_ASSERT_NOT_NULL(decoded);
    
    cJSON *task = cJSON_GetObjectItem(decoded, "task");
    TEST_ASSERT_EQUAL_STRING("/state_get", task->valuestring);
    
    cJSON *qid = cJSON_GetObjectItem(decoded, "qid");
    TEST_ASSERT_EQUAL_INT(42, qid->valueint);
    
    free(encoded);
    cJSON_Delete(original);
    cJSON_Delete(decoded);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_detect_msgpack);
    RUN_TEST(test_detect_json);
    RUN_TEST(test_roundtrip_msgpack);
    UNITY_END();
}

void loop() {}
```

Run tests with:
```bash
pio test -e esp32dev
```

### Integration Testing

Test with Python client:

```python
from uc2rest.mserial_v2 import SerialCommunicator

# Connect with MessagePack enabled
comm = SerialCommunicator(port='/dev/ttyUSB0', use_msgpack=True, debug=True)

# Test command
response = comm.send_command({"task": "/state_get"}, blocking=True)
print(f"Response: {response}")

# Verify MessagePack was used
stats = comm.get_statistics()
print(f"Messages sent: {stats['messages_sent']}")
```

## Performance Monitoring

Add debug output to track protocol usage:

```cpp
void logProtocolStats() {
    log_i("Protocol Stats:");
    log_i("  JSON messages: %d", jsonMessageCount);
    log_i("  MessagePack messages: %d", msgpackMessageCount);
    log_i("  Total bytes saved: %d", bytesSaved);
}
```

## Troubleshooting

### Issue: MessagePack decoding fails

**Solution:** Check mpack library version and ensure buffer size is adequate:
```cpp
#define MPACK_BUFFER_SIZE 4096  // Increase if needed
```

### Issue: Legacy clients break

**Solution:** Verify fallback logic:
```cpp
if (format == MessageFormat::UNKNOWN) {
    // Try legacy JSON parsing
    root = decodeLegacy(buffer, len);
}
```

### Issue: Memory leaks

**Solution:** Always free allocated buffers:
```cpp
uint8_t* encoded = SerialProtocol::encode(...);
if (encoded != nullptr) {
    // Use the buffer
    free(encoded);  // Don't forget!
}
```

## Migration Timeline

1. **Phase 1** (Current): Python support ready, ESP32 preparation
2. **Phase 2**: Add mpack library, implement SerialProtocol class
3. **Phase 3**: Update SerialProcess.cpp to use new protocol
4. **Phase 4**: Testing with real hardware
5. **Phase 5**: Deploy to production, monitor performance

## References

- mpack library: https://github.com/ludocode/mpack
- MessagePack spec: https://msgpack.org/
- Protocol V2 docs: See `SERIAL_PROTOCOL_V2.md`
