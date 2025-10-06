#include "BinaryProtocol.h"
#include "SerialProcess.h"
#include "../wifi/Endpoints.h"
#include "Arduino.h"

#ifdef ENABLE_BINARY_PROTOCOL

namespace BinaryProtocol {
    
    static BinaryProtocolMode currentMode = PROTOCOL_AUTO_DETECT;
    static bool initialized = false;
    
    void setup() {
        if (!initialized) {
            currentMode = PROTOCOL_AUTO_DETECT;
            initialized = true;
            log_i("Binary protocol initialized in auto-detect mode");
        }
    }
    
    bool isEnabled() {
        return initialized && (currentMode != PROTOCOL_JSON_ONLY);
    }
    
    void setMode(BinaryProtocolMode mode) {
        currentMode = mode;
        log_i("Binary protocol mode set to: %d", mode);
    }
    
    BinaryProtocolMode getMode() {
        return currentMode;
    }
    
    uint16_t calculateChecksum(const uint8_t* data, uint16_t length) {
        uint16_t checksum = 0;
        for (uint16_t i = 0; i < length; i++) {
            checksum += data[i];
        }
        return checksum;
    }
    
    bool validateMessage(const BinaryHeader* header, const uint8_t* payload) {
        // Check magic bytes
        if (header->magic_start != BINARY_PROTOCOL_MAGIC_START) {
            return false;
        }
        
        // Check version
        if (header->version != BINARY_PROTOCOL_VERSION) {
            log_w("Binary protocol version mismatch: got %d, expected %d", header->version, BINARY_PROTOCOL_VERSION);
            return false;
        }
        
        // Check payload length
        if (header->payload_length > BINARY_MAX_PAYLOAD_SIZE) {
            log_w("Binary message payload too large: %d bytes", header->payload_length);
            return false;
        }
        
        // Verify checksum
        if (header->payload_length > 0 && payload != nullptr) {
            uint16_t calculatedChecksum = calculateChecksum(payload, header->payload_length);
            if (calculatedChecksum != header->checksum) {
                log_w("Binary message checksum mismatch: got %d, expected %d", header->checksum, calculatedChecksum);
                return false;
            }
        }
        
        return true;
    }
    
    const char* commandIdToEndpoint(BinaryCommandId cmdId) {
        switch (cmdId) {
            case CMD_STATE_GET: return state_get_endpoint;
            case CMD_STATE_ACT: return state_act_endpoint;
            case CMD_STATE_BUSY: return state_busy_endpoint;
            
#ifdef MOTOR_CONTROLLER
            case CMD_MOTOR_GET: return motor_get_endpoint;
            case CMD_MOTOR_ACT: return motor_act_endpoint;
#endif

#ifdef LED_CONTROLLER
            case CMD_LED_GET: return ledarr_get_endpoint;
            case CMD_LED_ACT: return ledarr_act_endpoint;
#endif

#ifdef LASER_CONTROLLER
            case CMD_LASER_GET: return laser_get_endpoint;
            case CMD_LASER_ACT: return laser_act_endpoint;
#endif

#ifdef DIGITAL_OUT_CONTROLLER
            case CMD_DIGITAL_OUT_GET: return digitalout_get_endpoint;
            case CMD_DIGITAL_OUT_ACT: return digitalout_act_endpoint;
#endif

#ifdef DIGITAL_IN_CONTROLLER
            case CMD_DIGITAL_IN_GET: return digitalin_get_endpoint;
            case CMD_DIGITAL_IN_ACT: return digitalin_act_endpoint;
#endif
            
            default: return nullptr;
        }
    }
    
    BinaryCommandId endpointToCommandId(const char* endpoint) {
        if (strcmp(endpoint, state_get_endpoint) == 0) return CMD_STATE_GET;
        if (strcmp(endpoint, state_act_endpoint) == 0) return CMD_STATE_ACT;
        if (strcmp(endpoint, state_busy_endpoint) == 0) return CMD_STATE_BUSY;
        
#ifdef MOTOR_CONTROLLER
        if (strcmp(endpoint, motor_get_endpoint) == 0) return CMD_MOTOR_GET;
        if (strcmp(endpoint, motor_act_endpoint) == 0) return CMD_MOTOR_ACT;
#endif

#ifdef LED_CONTROLLER
        if (strcmp(endpoint, ledarr_get_endpoint) == 0) return CMD_LED_GET;
        if (strcmp(endpoint, ledarr_act_endpoint) == 0) return CMD_LED_ACT;
#endif

#ifdef LASER_CONTROLLER
        if (strcmp(endpoint, laser_get_endpoint) == 0) return CMD_LASER_GET;
        if (strcmp(endpoint, laser_act_endpoint) == 0) return CMD_LASER_ACT;
#endif

#ifdef DIGITAL_OUT_CONTROLLER
        if (strcmp(endpoint, digitalout_get_endpoint) == 0) return CMD_DIGITAL_OUT_GET;
        if (strcmp(endpoint, digitalout_act_endpoint) == 0) return CMD_DIGITAL_OUT_ACT;
#endif

#ifdef DIGITAL_IN_CONTROLLER
        if (strcmp(endpoint, digitalin_get_endpoint) == 0) return CMD_DIGITAL_IN_GET;
        if (strcmp(endpoint, digitalin_act_endpoint) == 0) return CMD_DIGITAL_IN_ACT;
#endif
        
        return CMD_UNKNOWN;
    }
    
    cJSON* binaryToJson(BinaryCommandId cmdId, const uint8_t* payload, uint16_t payloadLength) {
        // Create a basic JSON structure with the task endpoint
        cJSON* root = cJSON_CreateObject();
        if (!root) {
            return nullptr;
        }
        
        // Add the task endpoint
        const char* endpoint = commandIdToEndpoint(cmdId);
        if (endpoint) {
            cJSON_AddStringToObject(root, "task", endpoint);
        } else {
            cJSON_AddStringToObject(root, "task", "unknown");
        }
        
        // For now, create minimal JSON structures for different command types
        // In a full implementation, this would parse the binary payload into appropriate JSON
        switch (cmdId) {
            case CMD_STATE_BUSY:
                // No additional parameters needed for busy check
                break;
                
            case CMD_PROTOCOL_SWITCH:
                if (payloadLength >= 1) {
                    BinaryProtocolMode newMode = (BinaryProtocolMode)payload[0];
                    setMode(newMode);
                    cJSON_AddNumberToObject(root, "protocol_mode", newMode);
                }
                break;
                
            case CMD_PROTOCOL_INFO:
                cJSON_AddStringToObject(root, "protocol", "binary");
                cJSON_AddNumberToObject(root, "version", BINARY_PROTOCOL_VERSION);
                cJSON_AddNumberToObject(root, "mode", currentMode);
                break;
                
            default:
                // For other commands, we'd need to parse the binary payload
                // For now, just add a placeholder
                if (payloadLength > 0) {
                    cJSON_AddStringToObject(root, "binary_data", "present");
                }
                break;
        }
        
        return root;
    }
    
    void sendBinaryResponse(BinaryCommandId cmdId, uint8_t status, const uint8_t* payload, uint16_t payloadLength) {
        BinaryResponse response;
        response.magic_start = BINARY_PROTOCOL_MAGIC_START;
        response.version = BINARY_PROTOCOL_VERSION;
        response.command_id = cmdId;
        response.status = status;
        response.payload_length = payloadLength;
        response.checksum = (payload && payloadLength > 0) ? calculateChecksum(payload, payloadLength) : 0;
        response.magic_end = BINARY_PROTOCOL_MAGIC_END;
        
        // Send response header
        Serial.write((uint8_t*)&response, sizeof(BinaryResponse));
        
        // Send payload if present
        if (payload && payloadLength > 0) {
            Serial.write(payload, payloadLength);
        }
        
        Serial.flush();
    }
    
    void processBinaryMessage(const uint8_t* data, size_t length) {
        if (length < sizeof(BinaryHeader)) {
            log_w("Binary message too short: %d bytes", length);
            return;
        }
        
        const BinaryHeader* header = (const BinaryHeader*)data;
        const uint8_t* payload = (length > sizeof(BinaryHeader)) ? data + sizeof(BinaryHeader) : nullptr;
        
        // Validate the message
        if (!validateMessage(header, payload)) {
            log_w("Invalid binary message received");
            sendBinaryResponse((BinaryCommandId)header->command_id, 1, nullptr, 0); // Error status
            return;
        }
        
        // Handle protocol control commands directly
        if (header->command_id == CMD_PROTOCOL_SWITCH || header->command_id == CMD_PROTOCOL_INFO) {
            cJSON* json = binaryToJson((BinaryCommandId)header->command_id, payload, header->payload_length);
            if (json) {
                // Send success response with protocol info
                const char* response_str = cJSON_PrintUnformatted(json);
                if (response_str) {
                    sendBinaryResponse((BinaryCommandId)header->command_id, 0, (const uint8_t*)response_str, strlen(response_str));
                    free((void*)response_str);
                }
                cJSON_Delete(json);
            }
            return;
        }
        
        // Convert binary message to JSON and use existing processing pipeline
        cJSON* json = binaryToJson((BinaryCommandId)header->command_id, payload, header->payload_length);
        if (json) {
            // Add the JSON to the existing processing queue
            SerialProcess::addJsonToQueue(json);
            
            // Send acknowledgment (the actual response will come through the normal JSON pipeline)
            sendBinaryResponse((BinaryCommandId)header->command_id, 0, nullptr, 0); // Success status
        } else {
            log_w("Failed to convert binary message to JSON");
            sendBinaryResponse((BinaryCommandId)header->command_id, 1, nullptr, 0); // Error status
        }
    }
}

#endif // ENABLE_BINARY_PROTOCOL