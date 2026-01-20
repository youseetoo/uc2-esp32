/**
 * @file canopen_slave.h
 * @brief CANopen slave/satellite node implementation for UC2-ESP32
 * 
 * Provides CANopen functionality for satellite devices (motors, LEDs, lasers, sensors).
 * This layer sits on top of the existing ISO-TP transport and provides:
 * - NMT (Network Management) state machine
 * - Heartbeat generation
 * - Emergency message handling
 * - SDO server for parameter access
 * - PDO handling for real-time data
 * 
 * The implementation is designed to be minimal and non-intrusive, working
 * alongside the existing UC2 custom protocol for backward compatibility.
 */

#pragma once

#include "canopen_types.h"
#include <stdint.h>
#include <stdbool.h>

namespace CANopen_Slave
{
    // ========================================================================
    // Initialization and Configuration
    // ========================================================================
    
    /**
     * @brief Initialize CANopen slave node
     * @param nodeId CAN node ID (1-127)
     * @param deviceType CANopen device type identifier
     * @param heartbeatInterval_ms Heartbeat interval in milliseconds (0 = disabled)
     */
    void init(uint8_t nodeId, uint32_t deviceType, uint16_t heartbeatInterval_ms = 1000);
    
    /**
     * @brief Set heartbeat interval
     * @param interval_ms Interval in milliseconds (0 = disabled)
     */
    void setHeartbeatInterval(uint16_t interval_ms);
    
    /**
     * @brief Get current NMT state
     * @return Current NMT state
     */
    CANopen_NMT_State getNmtState();
    
    // ========================================================================
    // NMT (Network Management) Functions
    // ========================================================================
    
    /**
     * @brief Process incoming NMT command
     * @param msg Pointer to NMT message
     * @return true if command was processed
     */
    bool processNmtCommand(const CANopen_NMT_Message* msg);
    
    /**
     * @brief Manually set NMT state (internal use)
     * @param newState New NMT state
     */
    void setNmtState(CANopen_NMT_State newState);
    
    /**
     * @brief Send boot-up message
     * Should be called after device initialization is complete
     */
    void sendBootup();
    
    // ========================================================================
    // Heartbeat Functions
    // ========================================================================
    
    /**
     * @brief Update heartbeat (call periodically from main loop)
     * Sends heartbeat message when interval has elapsed
     */
    void updateHeartbeat();
    
    /**
     * @brief Send heartbeat message immediately
     */
    void sendHeartbeat();
    
    // ========================================================================
    // Emergency (EMCY) Functions
    // ========================================================================
    
    /**
     * @brief Send emergency message
     * @param errorCode Emergency error code
     * @param errorRegister Error register bitmap
     * @param mfrData Manufacturer-specific data (5 bytes, can be NULL)
     */
    void sendEmergency(uint16_t errorCode, uint8_t errorRegister, const uint8_t* mfrData = nullptr);
    
    /**
     * @brief Clear all errors and send "no error" EMCY
     */
    void clearEmergency();
    
    // ========================================================================
    // SDO (Service Data Object) Server Functions
    // ========================================================================
    
    /**
     * @brief Process incoming SDO request
     * @param msg Pointer to SDO message
     * @return true if SDO was processed and response sent
     */
    bool processSdoRequest(const CANopen_SDO_Message* msg);
    
    /**
     * @brief Send SDO response
     * @param msg Pointer to SDO response message to send
     */
    void sendSdoResponse(const CANopen_SDO_Message* msg);
    
    /**
     * @brief Send SDO abort message
     * @param index Object dictionary index
     * @param subIndex Object dictionary sub-index
     * @param abortCode SDO abort code
     */
    void sendSdoAbort(uint16_t index, uint8_t subIndex, uint32_t abortCode);
    
    // ========================================================================
    // PDO (Process Data Object) Functions
    // ========================================================================
    
    /**
     * @brief Check if PDOs are enabled (depends on NMT state)
     * @return true if in OPERATIONAL state
     */
    bool arePdosEnabled();
    
    /**
     * @brief Process incoming RPDO (Receive PDO)
     * @param cobId COB-ID of the PDO
     * @param data Pointer to PDO data
     * @param length Length of PDO data
     * @return true if PDO was processed
     */
    bool processRpdo(uint16_t cobId, const uint8_t* data, uint8_t length);
    
    /**
     * @brief Send TPDO (Transmit PDO)
     * @param pdoNum PDO number (1-4)
     * @param data Pointer to PDO data
     * @param length Length of PDO data
     */
    void sendTpdo(uint8_t pdoNum, const uint8_t* data, uint8_t length);
    
    // ========================================================================
    // Object Dictionary Access Functions
    // ========================================================================
    
    /**
     * @brief Read from Object Dictionary
     * @param index OD index
     * @param subIndex OD sub-index
     * @param data Buffer to store read data
     * @param dataSize Pointer to data size (in: buffer size, out: actual size)
     * @return SDO abort code (0 = success)
     */
    uint32_t odRead(uint16_t index, uint8_t subIndex, uint8_t* data, uint8_t* dataSize);
    
    /**
     * @brief Write to Object Dictionary
     * @param index OD index
     * @param subIndex OD sub-index
     * @param data Data to write
     * @param dataSize Size of data to write
     * @return SDO abort code (0 = success)
     */
    uint32_t odWrite(uint16_t index, uint8_t subIndex, const uint8_t* data, uint8_t dataSize);
    
    // ========================================================================
    // Main Loop Function
    // ========================================================================
    
    /**
     * @brief Main CANopen slave loop function
     * Call this periodically from main loop to handle:
     * - Heartbeat generation
     * - State transitions
     * - Periodic PDO transmission
     */
    void loop();
    
    // ========================================================================
    // Device-Specific PDO Helpers
    // ========================================================================
    
    #ifdef MOTOR_CONTROLLER
    /**
     * @brief Send motor status TPDO
     * @param position Current motor position
     * @param statusWord Motor status bitmap
     */
    void sendMotorStatus(int32_t position, uint8_t statusWord);
    
    /**
     * @brief Process motor command RPDO
     * @param targetPos Target position
     * @param speed Motor speed
     */
    void processMotorCommand(int32_t targetPos, uint32_t speed);
    #endif
    
    #ifdef LASER_CONTROLLER
    /**
     * @brief Process laser command RPDO
     * @param intensity Laser intensity
     * @param laserId Laser channel ID
     * @param enable Enable/disable flag
     */
    void processLaserCommand(uint16_t intensity, uint8_t laserId, uint8_t enable);
    #endif
    
    #ifdef LED_CONTROLLER
    /**
     * @brief Process LED command RPDO
     * @param mode LED mode
     * @param r Red channel
     * @param g Green channel
     * @param b Blue channel
     * @param brightness Overall brightness
     */
    void processLedCommand(uint8_t mode, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
    #endif
    
} // namespace CANopen_Slave
