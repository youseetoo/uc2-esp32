/**
 * @file canopen_master.h
 * @brief CANopen master/gateway node implementation for UC2-ESP32
 * 
 * Provides CANopen functionality for the master/gateway device that:
 * - Receives commands via Serial/USB from host computer
 * - Manages satellite devices using CANopen NMT
 * - Translates commands to CAN bus messages
 * - Reports device states back to host
 * - Implements network scanning and device discovery
 * 
 * The master node acts as a bridge between the serial interface and the
 * CAN bus network, providing a unified interface for controlling all
 * satellite devices.
 */

#pragma once

#include "canopen_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <cJSON.h>

namespace CANopen_Master
{
    // ========================================================================
    // Initialization and Configuration
    // ========================================================================
    
    /**
     * @brief Initialize CANopen master node
     */
    void init();
    
    /**
     * @brief Set master node ID (typically 0 for broadcast commands)
     * @param nodeId Master node ID
     */
    void setNodeId(uint8_t nodeId);
    
    // ========================================================================
    // NMT (Network Management) Functions
    // ========================================================================
    
    /**
     * @brief Send NMT command to a specific node or all nodes
     * @param command NMT command to send
     * @param targetNodeId Target node ID (0 = broadcast to all)
     * @return 0 on success, -1 on error
     */
    int sendNmtCommand(CANopen_NMT_Command command, uint8_t targetNodeId = 0);
    
    /**
     * @brief Start a remote node (enter operational state)
     * @param nodeId Target node ID (0 = all nodes)
     * @return 0 on success, -1 on error
     */
    int startNode(uint8_t nodeId = 0);
    
    /**
     * @brief Stop a remote node (enter stopped state)
     * @param nodeId Target node ID (0 = all nodes)
     * @return 0 on success, -1 on error
     */
    int stopNode(uint8_t nodeId = 0);
    
    /**
     * @brief Put a remote node into pre-operational state
     * @param nodeId Target node ID (0 = all nodes)
     * @return 0 on success, -1 on error
     */
    int enterPreoperational(uint8_t nodeId = 0);
    
    /**
     * @brief Reset a remote node (software reset)
     * @param nodeId Target node ID
     * @return 0 on success, -1 on error
     */
    int resetNode(uint8_t nodeId);
    
    /**
     * @brief Reset communication parameters of a remote node
     * @param nodeId Target node ID
     * @return 0 on success, -1 on error
     */
    int resetCommunication(uint8_t nodeId);
    
    // ========================================================================
    // Node Monitoring Functions
    // ========================================================================
    
    /**
     * @brief Check if a node is online (based on heartbeat)
     * @param nodeId Node ID to check
     * @return true if node is online
     */
    bool isNodeOnline(uint8_t nodeId);
    
    /**
     * @brief Get NMT state of a remote node
     * @param nodeId Node ID
     * @return NMT state (NMT_STATE_BOOTUP if unknown)
     */
    CANopen_NMT_State getNodeState(uint8_t nodeId);
    
    /**
     * @brief Update node status from heartbeat message
     * @param nodeId Node ID
     * @param state NMT state from heartbeat
     */
    void updateNodeHeartbeat(uint8_t nodeId, CANopen_NMT_State state);
    
    /**
     * @brief Check for nodes that haven't sent heartbeat (timeout)
     * @param timeout_ms Timeout in milliseconds
     */
    void checkHeartbeatTimeouts(uint32_t timeout_ms = 5000);
    
    // ========================================================================
    // SDO (Service Data Object) Client Functions
    // ========================================================================
    
    /**
     * @brief Read from Object Dictionary of a remote node (SDO upload)
     * @param nodeId Target node ID
     * @param index OD index
     * @param subIndex OD sub-index
     * @param data Buffer to store read data
     * @param dataSize Pointer to data size (in: buffer size, out: actual size)
     * @param timeout_ms Timeout in milliseconds
     * @return 0 on success, SDO abort code on error
     */
    uint32_t sdoRead(uint8_t nodeId, uint16_t index, uint8_t subIndex, 
                     uint8_t* data, uint8_t* dataSize, uint32_t timeout_ms = 1000);
    
    /**
     * @brief Write to Object Dictionary of a remote node (SDO download)
     * @param nodeId Target node ID
     * @param index OD index
     * @param subIndex OD sub-index
     * @param data Data to write
     * @param dataSize Size of data
     * @param timeout_ms Timeout in milliseconds
     * @return 0 on success, SDO abort code on error
     */
    uint32_t sdoWrite(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                      const uint8_t* data, uint8_t dataSize, uint32_t timeout_ms = 1000);
    
    /**
     * @brief Process incoming SDO response
     * @param nodeId Source node ID
     * @param msg Pointer to SDO message
     * @return true if response was processed
     */
    bool processSdoResponse(uint8_t nodeId, const CANopen_SDO_Message* msg);
    
    // ========================================================================
    // PDO Functions
    // ========================================================================
    
    /**
     * @brief Send RPDO to a node
     * @param nodeId Target node ID
     * @param pdoNum PDO number (1-4)
     * @param data Pointer to PDO data
     * @param length Length of PDO data
     * @return 0 on success, -1 on error
     */
    int sendRpdo(uint8_t nodeId, uint8_t pdoNum, const uint8_t* data, uint8_t length);
    
    /**
     * @brief Process incoming TPDO from a node
     * @param nodeId Source node ID
     * @param pdoNum PDO number (1-4)
     * @param data Pointer to PDO data
     * @param length Length of PDO data
     * @return true if PDO was processed
     */
    bool processTpdo(uint8_t nodeId, uint8_t pdoNum, const uint8_t* data, uint8_t length);
    
    // ========================================================================
    // Network Scan and Discovery
    // ========================================================================
    
    /**
     * @brief Scan CAN network for available devices using CANopen NMT
     * Sends NMT commands and collects boot-up/heartbeat responses
     * @param timeout_ms Scan timeout in milliseconds
     * @return cJSON array of discovered devices
     */
    cJSON* scanNetwork(uint32_t timeout_ms = 2000);
    
    /**
     * @brief Request device information via SDO
     * @param nodeId Node ID to query
     * @return cJSON object with device info (vendor ID, product code, etc.)
     */
    cJSON* getDeviceInfo(uint8_t nodeId);
    
    // ========================================================================
    // Device Control Functions (High-Level API)
    // ========================================================================
    
    /**
     * @brief Send motor command to a motor controller node
     * @param nodeId Motor controller node ID
     * @param targetPos Target position in steps
     * @param speed Speed in steps/sec
     * @return 0 on success, -1 on error
     */
    int sendMotorCommand(uint8_t nodeId, int32_t targetPos, uint32_t speed);
    
    /**
     * @brief Send laser command to a laser controller node
     * @param nodeId Laser controller node ID
     * @param intensity Laser intensity (0-65535)
     * @param laserId Laser channel ID
     * @param enable Enable/disable laser
     * @return 0 on success, -1 on error
     */
    int sendLaserCommand(uint8_t nodeId, uint16_t intensity, uint8_t laserId, uint8_t enable);
    
    /**
     * @brief Send LED command to an LED controller node
     * @param nodeId LED controller node ID
     * @param mode LED mode
     * @param r Red channel (0-255)
     * @param g Green channel (0-255)
     * @param b Blue channel (0-255)
     * @param brightness Overall brightness (0-255)
     * @return 0 on success, -1 on error
     */
    int sendLedCommand(uint8_t nodeId, uint8_t mode, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
    
    // ========================================================================
    // Serial Gateway Functions
    // ========================================================================
    
    /**
     * @brief Process JSON command from serial interface
     * Translates serial commands to CANopen messages
     * @param doc JSON document with command
     * @return qid (query ID) from command, or -1 on error
     */
    int processSerialCommand(cJSON* doc);
    
    /**
     * @brief Send device status to serial interface
     * @param nodeId Node ID
     * @param statusJson JSON object with status data
     */
    void sendStatusToSerial(uint8_t nodeId, cJSON* statusJson);
    
    // ========================================================================
    // Emergency Handling
    // ========================================================================
    
    /**
     * @brief Process incoming emergency message
     * @param nodeId Source node ID
     * @param emcy Pointer to emergency message
     */
    void processEmergency(uint8_t nodeId, const CANopen_EMCY_Message* emcy);
    
    /**
     * @brief Get error status for a node
     * @param nodeId Node ID
     * @return Error register value
     */
    uint8_t getNodeErrorStatus(uint8_t nodeId);
    
    // ========================================================================
    // Main Loop Function
    // ========================================================================
    
    /**
     * @brief Main CANopen master loop function
     * Call this periodically from main loop to handle:
     * - Heartbeat monitoring
     * - SDO response handling
     * - Network management
     */
    void loop();
    
    // ========================================================================
    // Statistics and Diagnostics
    // ========================================================================
    
    /**
     * @brief Get network statistics
     * @return cJSON object with network stats (active nodes, messages sent/received, etc.)
     */
    cJSON* getNetworkStats();
    
    /**
     * @brief Reset network statistics
     */
    void resetNetworkStats();
    
} // namespace CANopen_Master
