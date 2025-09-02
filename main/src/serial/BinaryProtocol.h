#pragma once

#include "Arduino.h"
#include "cJSON.h"

// Enable binary protocol support - can be overridden by build flags
#ifndef ENABLE_BINARY_PROTOCOL
#define ENABLE_BINARY_PROTOCOL 1
#endif

#ifdef ENABLE_BINARY_PROTOCOL

// Binary protocol magic bytes to identify binary vs JSON messages
#define BINARY_PROTOCOL_MAGIC_START 0xAB
#define BINARY_PROTOCOL_MAGIC_END 0xCD
#define BINARY_PROTOCOL_VERSION 0x01

// Maximum binary message size
#define BINARY_MAX_MESSAGE_SIZE 1024
#define BINARY_MAX_PAYLOAD_SIZE (BINARY_MAX_MESSAGE_SIZE - sizeof(BinaryHeader))

// Binary protocol command IDs - mapping to existing endpoints
enum BinaryCommandId : uint8_t {
    // State commands
    CMD_STATE_GET = 0x01,
    CMD_STATE_ACT = 0x02,
    CMD_STATE_BUSY = 0x03,
    
    // Motor commands
    CMD_MOTOR_GET = 0x10,
    CMD_MOTOR_ACT = 0x11,
    
    // LED commands
    CMD_LED_GET = 0x20,
    CMD_LED_ACT = 0x21,
    
    // Laser commands
    CMD_LASER_GET = 0x30,
    CMD_LASER_ACT = 0x31,
    
    // Digital IO commands
    CMD_DIGITAL_OUT_GET = 0x40,
    CMD_DIGITAL_OUT_ACT = 0x41,
    CMD_DIGITAL_IN_GET = 0x42,
    CMD_DIGITAL_IN_ACT = 0x43,
    
    // Protocol control commands
    CMD_PROTOCOL_SWITCH = 0xF0,
    CMD_PROTOCOL_INFO = 0xF1,
    
    // Error/Unknown command
    CMD_UNKNOWN = 0xFF
};

// Binary protocol operating modes
enum BinaryProtocolMode : uint8_t {
    PROTOCOL_JSON_ONLY = 0,
    PROTOCOL_BINARY_ONLY = 1,
    PROTOCOL_AUTO_DETECT = 2  // Default - auto-detect based on message format
};

// Binary message header structure
struct __attribute__((packed)) BinaryHeader {
    uint8_t magic_start;     // BINARY_PROTOCOL_MAGIC_START (0xAB)
    uint8_t version;         // Protocol version
    uint8_t command_id;      // BinaryCommandId
    uint8_t flags;           // Reserved for future use
    uint16_t payload_length; // Length of payload data
    uint16_t checksum;       // Simple checksum of payload
};

// Binary response header
struct __attribute__((packed)) BinaryResponse {
    uint8_t magic_start;     // BINARY_PROTOCOL_MAGIC_START (0xAB)
    uint8_t version;         // Protocol version  
    uint8_t command_id;      // Original command ID
    uint8_t status;          // 0=success, 1=error, 2=busy
    uint16_t payload_length; // Length of response payload
    uint16_t checksum;       // Simple checksum of payload
    uint8_t magic_end;       // BINARY_PROTOCOL_MAGIC_END (0xCD)
};

namespace BinaryProtocol {
    // Initialize binary protocol subsystem
    void setup();
    
    // Check if binary protocol is enabled
    bool isEnabled();
    
    // Set protocol mode
    void setMode(BinaryProtocolMode mode);
    BinaryProtocolMode getMode();
    
    // Parse and process binary message
    void processBinaryMessage(const uint8_t* data, size_t length);
    
    // Convert binary command to JSON for existing controller processing
    cJSON* binaryToJson(BinaryCommandId cmdId, const uint8_t* payload, uint16_t payloadLength);
    
    // Send binary response
    void sendBinaryResponse(BinaryCommandId cmdId, uint8_t status, const uint8_t* payload = nullptr, uint16_t payloadLength = 0);
    
    // Utility functions
    uint16_t calculateChecksum(const uint8_t* data, uint16_t length);
    bool validateMessage(const BinaryHeader* header, const uint8_t* payload);
    
    // Command ID mapping functions
    BinaryCommandId endpointToCommandId(const char* endpoint);
    const char* commandIdToEndpoint(BinaryCommandId cmdId);
};

#endif // ENABLE_BINARY_PROTOCOL