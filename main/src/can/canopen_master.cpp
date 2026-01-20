/**
 * @file canopen_master.cpp
 * @brief CANopen master/gateway node implementation for UC2-ESP32
 */

#include "canopen_master.h"
#include "can_controller.h"
#include <PinConfig.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "../serial/SerialProcess.h"

namespace CANopen_Master
{
    // ========================================================================
    // Private State Variables
    // ========================================================================
    
    static uint8_t masterNodeId = 0;  // Master typically uses 0 for broadcast
    static const int MAX_NODES = 127;
    
    // Node tracking structure
    struct NodeInfo {
        bool online;
        CANopen_NMT_State state;
        unsigned long lastHeartbeat;
        uint8_t errorRegister;
        uint32_t deviceType;
    };
    
    static NodeInfo nodes[MAX_NODES + 1];  // Index by node ID (1-127)
    
    // Network statistics
    struct NetworkStats {
        uint32_t nmtCommandsSent;
        uint32_t sdoRequestsSent;
        uint32_t sdoResponsesReceived;
        uint32_t pdosSent;
        uint32_t pdosReceived;
        uint32_t heartbeatsReceived;
        uint32_t emergenciesReceived;
    };
    
    static NetworkStats stats;
    
    // ========================================================================
    // Helper Functions
    // ========================================================================
    
    /**
     * @brief Send CAN message with standard 11-bit identifier
     */
    static int sendCanOpenMessage(uint16_t cobId, const uint8_t* data, uint8_t length)
    {
        return can_controller::sendCanMessage(cobId, data, length);
    }
    
    // ========================================================================
    // Public Functions - Initialization
    // ========================================================================
    
    void init()
    {
        masterNodeId = 0;
        memset(&nodes, 0, sizeof(nodes));
        memset(&stats, 0, sizeof(stats));
        
        log_i("CANopen Master initialized");
    }
    
    void setNodeId(uint8_t nodeId)
    {
        masterNodeId = nodeId;
    }
    
    // ========================================================================
    // NMT Functions
    // ========================================================================
    
    int sendNmtCommand(CANopen_NMT_Command command, uint8_t targetNodeId)
    {
        CANopen_NMT_Message msg;
        msg.command = command;
        msg.nodeId = targetNodeId;
        
        int result = sendCanOpenMessage(CANOPEN_FUNC_NMT, (const uint8_t*)&msg, sizeof(msg));
        
        if (result == 0) {
            stats.nmtCommandsSent++;
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Sent NMT command 0x%02X to node %u", command, targetNodeId);
        }
        
        return result;
    }
    
    int startNode(uint8_t nodeId)
    {
        return sendNmtCommand(NMT_CMD_START_REMOTE_NODE, nodeId);
    }
    
    int stopNode(uint8_t nodeId)
    {
        return sendNmtCommand(NMT_CMD_STOP_REMOTE_NODE, nodeId);
    }
    
    int enterPreoperational(uint8_t nodeId)
    {
        return sendNmtCommand(NMT_CMD_ENTER_PREOPERATIONAL, nodeId);
    }
    
    int resetNode(uint8_t nodeId)
    {
        if (nodeId == 0) {
            log_w("Cannot broadcast reset command");
            return -1;
        }
        return sendNmtCommand(NMT_CMD_RESET_NODE, nodeId);
    }
    
    int resetCommunication(uint8_t nodeId)
    {
        if (nodeId == 0) {
            log_w("Cannot broadcast reset communication command");
            return -1;
        }
        return sendNmtCommand(NMT_CMD_RESET_COMMUNICATION, nodeId);
    }
    
    // ========================================================================
    // Node Monitoring Functions
    // ========================================================================
    
    bool isNodeOnline(uint8_t nodeId)
    {
        if (nodeId == 0 || nodeId > MAX_NODES) return false;
        return nodes[nodeId].online;
    }
    
    CANopen_NMT_State getNodeState(uint8_t nodeId)
    {
        if (nodeId == 0 || nodeId > MAX_NODES) return NMT_STATE_BOOTUP;
        return nodes[nodeId].state;
    }
    
    void updateNodeHeartbeat(uint8_t nodeId, CANopen_NMT_State state)
    {
        if (nodeId == 0 || nodeId > MAX_NODES) return;
        
        bool wasOffline = !nodes[nodeId].online;
        
        nodes[nodeId].online = true;
        nodes[nodeId].state = state;
        nodes[nodeId].lastHeartbeat = millis();
        stats.heartbeatsReceived++;
        
        if (wasOffline) {
            log_i("Node %u came online (state: %u)", nodeId, state);
            can_controller::addCANDeviceToAvailableList(nodeId);
        }
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Heartbeat from node %u: state=%u", nodeId, state);
    }
    
    void checkHeartbeatTimeouts(uint32_t timeout_ms)
    {
        unsigned long currentTime = millis();
        
        for (int i = 1; i <= MAX_NODES; i++) {
            if (nodes[i].online) {
                if (currentTime - nodes[i].lastHeartbeat > timeout_ms) {
                    log_w("Node %u heartbeat timeout", i);
                    nodes[i].online = false;
                    can_controller::removeCANDeviceFromAvailableList(i);
                }
            }
        }
    }
    
    // ========================================================================
    // SDO Client Functions (Simplified)
    // ========================================================================
    
    uint32_t sdoRead(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                     uint8_t* data, uint8_t* dataSize, uint32_t timeout_ms)
    {
        if (nodeId == 0 || nodeId > MAX_NODES) {
            return SDO_ABORT_GENERAL_ERROR;
        }
        
        // Prepare SDO upload (read) request
        CANopen_SDO_Message request;
        request.commandSpecifier = SDO_CCS_UPLOAD_INITIATE;
        request.index = index;
        request.subIndex = subIndex;
        memset(request.data, 0, 4);
        
        uint16_t cobId = CANOPEN_FUNC_SDO_RX + nodeId;
        int result = sendCanOpenMessage(cobId, (const uint8_t*)&request, sizeof(request));
        
        if (result != 0) {
            return SDO_ABORT_GENERAL_ERROR;
        }
        
        stats.sdoRequestsSent++;
        
        // TODO: Implement synchronous SDO transfer with response handling
        // This is a placeholder implementation. For full functionality:
        // 1. Add response queue and wait for SDO_TX message
        // 2. Implement timeout handling
        // 3. Parse response and extract data
        // Current implementation sends request but doesn't wait for response
        
        log_i("SDO read request sent to node %u: index=0x%04X, subIndex=%u", 
              nodeId, index, subIndex);
        
        // Return timeout error as we don't have response handling yet
        return SDO_ABORT_TIMEOUT;
    }
    
    uint32_t sdoWrite(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                      const uint8_t* data, uint8_t dataSize, uint32_t timeout_ms)
    {
        if (nodeId == 0 || nodeId > MAX_NODES || dataSize > 4) {
            return SDO_ABORT_GENERAL_ERROR;
        }
        
        // Prepare SDO download (write) request
        CANopen_SDO_Message request;
        request.commandSpecifier = SDO_CCS_DOWNLOAD_INITIATE | SDO_EXPEDITED_FLAG | SDO_SIZE_INDICATED;
        
        // Set size bits
        uint8_t n = 4 - dataSize;
        request.commandSpecifier |= (n << 2);
        
        request.index = index;
        request.subIndex = subIndex;
        memcpy(request.data, data, dataSize);
        if (dataSize < 4) {
            memset(request.data + dataSize, 0, 4 - dataSize);
        }
        
        uint16_t cobId = CANOPEN_FUNC_SDO_RX + nodeId;
        int result = sendCanOpenMessage(cobId, (const uint8_t*)&request, sizeof(request));
        
        if (result != 0) {
            return SDO_ABORT_GENERAL_ERROR;
        }
        
        stats.sdoRequestsSent++;
        
        log_i("SDO write request sent to node %u: index=0x%04X, subIndex=%u, size=%u",
              nodeId, index, subIndex, dataSize);
        
        return SDO_ABORT_TIMEOUT;  // Placeholder
    }
    
    bool processSdoResponse(uint8_t nodeId, const CANopen_SDO_Message* msg)
    {
        if (msg == nullptr) return false;
        
        stats.sdoResponsesReceived++;
        
        uint8_t cs = msg->commandSpecifier;
        
        // Check for abort
        if (cs == SDO_SCS_ABORT) {
            log_e("SDO abort from node %u: index=0x%04X, code=0x%08X",
                  nodeId, msg->index, msg->abortCode);
            return true;
        }
        
        // Handle upload (read) response
        if ((cs & 0x60) == SDO_SCS_UPLOAD_RESPONSE) {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("SDO upload response from node %u: index=0x%04X", nodeId, msg->index);
            return true;
        }
        
        // Handle download (write) confirmation
        if (cs == SDO_SCS_DOWNLOAD_RESPONSE) {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("SDO download confirmed from node %u: index=0x%04X", nodeId, msg->index);
            return true;
        }
        
        return false;
    }
    
    // ========================================================================
    // PDO Functions
    // ========================================================================
    
    int sendRpdo(uint8_t nodeId, uint8_t pdoNum, const uint8_t* data, uint8_t length)
    {
        if (nodeId == 0 || nodeId > MAX_NODES || pdoNum < 1 || pdoNum > 4) {
            return -1;
        }
        
        uint16_t base = 0;
        switch (pdoNum) {
            case 1: base = CANOPEN_FUNC_RPDO1; break;
            case 2: base = CANOPEN_FUNC_RPDO2; break;
            case 3: base = CANOPEN_FUNC_RPDO3; break;
            case 4: base = CANOPEN_FUNC_RPDO4; break;
            default: return -1;
        }
        
        uint16_t cobId = base + nodeId;
        int result = sendCanOpenMessage(cobId, data, length);
        
        if (result == 0) {
            stats.pdosSent++;
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Sent RPDO%u to node %u (COB-ID: 0x%03X)", pdoNum, nodeId, cobId);
        }
        
        return result;
    }
    
    bool processTpdo(uint8_t nodeId, uint8_t pdoNum, const uint8_t* data, uint8_t length)
    {
        if (data == nullptr) return false;
        
        stats.pdosReceived++;
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Received TPDO%u from node %u (length: %u)", pdoNum, nodeId, length);
        
        // Forward to appropriate handler based on device type
        // This would integrate with existing motor/laser/LED controllers
        
        return true;
    }
    
    // ========================================================================
    // Network Scan
    // ========================================================================
    
    cJSON* scanNetwork(uint32_t timeout_ms)
    {
        log_i("Starting CANopen network scan...");
        
        // Reset all node states
        for (int i = 1; i <= MAX_NODES; i++) {
            nodes[i].online = false;
        }
        
        // Send NMT command to enter pre-operational (triggers heartbeat responses)
        sendNmtCommand(NMT_CMD_ENTER_PREOPERATIONAL, 0);  // Broadcast
        
        // Wait for heartbeat responses
        unsigned long startTime = millis();
        while (millis() - startTime < timeout_ms) {
            vTaskDelay(pdMS_TO_TICKS(10));
            // Heartbeat responses will be collected by updateNodeHeartbeat()
        }
        
        // Build response JSON
        cJSON* devicesArray = cJSON_CreateArray();
        if (devicesArray == NULL) {
            return NULL;
        }
        
        int deviceCount = 0;
        for (int i = 1; i <= MAX_NODES; i++) {
            if (nodes[i].online) {
                cJSON* deviceObj = cJSON_CreateObject();
                if (deviceObj != NULL) {
                    cJSON_AddNumberToObject(deviceObj, "nodeId", i);
                    cJSON_AddNumberToObject(deviceObj, "state", nodes[i].state);
                    cJSON_AddNumberToObject(deviceObj, "errorRegister", nodes[i].errorRegister);
                    
                    // Add state string
                    const char* stateStr = "unknown";
                    switch (nodes[i].state) {
                        case NMT_STATE_BOOTUP: stateStr = "bootup"; break;
                        case NMT_STATE_STOPPED: stateStr = "stopped"; break;
                        case NMT_STATE_OPERATIONAL: stateStr = "operational"; break;
                        case NMT_STATE_PREOPERATIONAL: stateStr = "preoperational"; break;
                    }
                    cJSON_AddStringToObject(deviceObj, "stateStr", stateStr);
                    
                    cJSON_AddItemToArray(devicesArray, deviceObj);
                    deviceCount++;
                }
            }
        }
        
        log_i("CANopen scan completed. Found %d devices.", deviceCount);
        return devicesArray;
    }
    
    cJSON* getDeviceInfo(uint8_t nodeId)
    {
        // Placeholder for SDO-based device info retrieval
        cJSON* info = cJSON_CreateObject();
        if (info != NULL) {
            cJSON_AddNumberToObject(info, "nodeId", nodeId);
            cJSON_AddBoolToObject(info, "online", isNodeOnline(nodeId));
            if (isNodeOnline(nodeId)) {
                cJSON_AddNumberToObject(info, "state", nodes[nodeId].state);
            }
        }
        return info;
    }
    
    // ========================================================================
    // High-Level Device Control
    // ========================================================================
    
    int sendMotorCommand(uint8_t nodeId, int32_t targetPos, uint32_t speed)
    {
        CANopen_Motor_RPDO1 cmd;
        cmd.targetPosition = targetPos;
        cmd.speed = speed;
        
        return sendRpdo(nodeId, 1, (const uint8_t*)&cmd, sizeof(cmd));
    }
    
    int sendLaserCommand(uint8_t nodeId, uint16_t intensity, uint8_t laserId, uint8_t enable)
    {
        CANopen_Laser_RPDO1 cmd;
        cmd.intensity = intensity;
        cmd.laserId = laserId;
        cmd.enable = enable;
        memset(cmd.reserved, 0, sizeof(cmd.reserved));
        
        return sendRpdo(nodeId, 1, (const uint8_t*)&cmd, sizeof(cmd));
    }
    
    int sendLedCommand(uint8_t nodeId, uint8_t mode, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
    {
        CANopen_LED_RPDO1 cmd;
        cmd.mode = mode;
        cmd.red = r;
        cmd.green = g;
        cmd.blue = b;
        cmd.brightness = brightness;
        memset(cmd.reserved, 0, sizeof(cmd.reserved));
        
        return sendRpdo(nodeId, 1, (const uint8_t*)&cmd, sizeof(cmd));
    }
    
    // ========================================================================
    // Serial Gateway
    // ========================================================================
    
    int processSerialCommand(cJSON* doc)
    {
        // Placeholder for serial command processing
        // This would parse JSON commands and translate to CANopen
        return -1;
    }
    
    void sendStatusToSerial(uint8_t nodeId, cJSON* statusJson)
    {
        if (statusJson == NULL) return;
        
        cJSON_AddNumberToObject(statusJson, "nodeId", nodeId);
        
        char* jsonString = cJSON_PrintUnformatted(statusJson);
        if (jsonString != NULL) {
            SerialProcess::safeSendJsonString(jsonString);
            free(jsonString);
        }
    }
    
    // ========================================================================
    // Emergency Handling
    // ========================================================================
    
    void processEmergency(uint8_t nodeId, const CANopen_EMCY_Message* emcy)
    {
        if (emcy == nullptr || nodeId == 0 || nodeId > MAX_NODES) return;
        
        stats.emergenciesReceived++;
        nodes[nodeId].errorRegister = emcy->errorRegister;
        
        log_e("Emergency from node %u: code=0x%04X, register=0x%02X",
              nodeId, emcy->errorCode, emcy->errorRegister);
        
        // Send emergency notification to serial
        cJSON* emcyJson = cJSON_CreateObject();
        if (emcyJson != NULL) {
            cJSON_AddStringToObject(emcyJson, "type", "emergency");
            cJSON_AddNumberToObject(emcyJson, "nodeId", nodeId);
            cJSON_AddNumberToObject(emcyJson, "errorCode", emcy->errorCode);
            cJSON_AddNumberToObject(emcyJson, "errorRegister", emcy->errorRegister);
            
            sendStatusToSerial(nodeId, emcyJson);
            cJSON_Delete(emcyJson);
        }
    }
    
    uint8_t getNodeErrorStatus(uint8_t nodeId)
    {
        if (nodeId == 0 || nodeId > MAX_NODES) return 0xFF;
        return nodes[nodeId].errorRegister;
    }
    
    // ========================================================================
    // Main Loop
    // ========================================================================
    
    void loop()
    {
        // Check for heartbeat timeouts
        checkHeartbeatTimeouts(5000);
        
        // Additional periodic tasks
    }
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    cJSON* getNetworkStats()
    {
        cJSON* statsJson = cJSON_CreateObject();
        if (statsJson != NULL) {
            int activeNodes = 0;
            for (int i = 1; i <= MAX_NODES; i++) {
                if (nodes[i].online) activeNodes++;
            }
            
            cJSON_AddNumberToObject(statsJson, "activeNodes", activeNodes);
            cJSON_AddNumberToObject(statsJson, "nmtCommandsSent", stats.nmtCommandsSent);
            cJSON_AddNumberToObject(statsJson, "sdoRequestsSent", stats.sdoRequestsSent);
            cJSON_AddNumberToObject(statsJson, "sdoResponsesReceived", stats.sdoResponsesReceived);
            cJSON_AddNumberToObject(statsJson, "pdosSent", stats.pdosSent);
            cJSON_AddNumberToObject(statsJson, "pdosReceived", stats.pdosReceived);
            cJSON_AddNumberToObject(statsJson, "heartbeatsReceived", stats.heartbeatsReceived);
            cJSON_AddNumberToObject(statsJson, "emergenciesReceived", stats.emergenciesReceived);
        }
        return statsJson;
    }
    
    void resetNetworkStats()
    {
        memset(&stats, 0, sizeof(stats));
        log_i("Network statistics reset");
    }
    
} // namespace CANopen_Master
