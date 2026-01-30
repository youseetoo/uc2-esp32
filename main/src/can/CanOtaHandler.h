/**
 * @file CanOtaHandler.h
 * @brief CAN-based OTA firmware update handler for ESP32
 * 
 * This module handles receiving firmware updates over CAN bus using ISO-TP protocol.
 * It receives firmware chunks from the master, verifies integrity, and writes to flash.
 * 
 * Integration:
 * - Include in can_controller.cpp
 * - Call handleCanOtaMessage() in dispatchIsoTpData()
 * - Call canOtaLoop() in main loop for timeout handling
 */

#pragma once

#include <Arduino.h>
#include "CanOtaTypes.h"
#include "cJSON.h"

// Use enum values from can_messagetype.h (included in can_controller.h)
// OTA_CAN_START = 0x62, OTA_CAN_DATA = 0x63, etc.

namespace can_ota {

// ============================================================================
// Constants
// ============================================================================

/**
 * @brief Timeout for receiving next chunk (milliseconds)
 * If no chunk received within this time, OTA is aborted
 */
constexpr uint32_t CHUNK_TIMEOUT_MS = 120000;  // 120s timeout for debugging

// ============================================================================
// Function Declarations
// ============================================================================

/**
 * @brief Initialize CAN OTA handler
 */
void init();

/**
 * @brief Main entry point for CAN OTA messages (slave side)
 * 
 * Call this from dispatchIsoTpData() when receiving OTA message types
 * 
 * @param messageType CANOtaMessageType
 * @param data Pointer to message payload (after message type byte)
 * @param len Length of payload
 * @param sourceCanId CAN ID of sender (for responses)
 */
void handleCanOtaMessage(uint8_t messageType, const uint8_t* data, size_t len, uint8_t sourceCanId);

/**
 * @brief Handle OTA START command (slave side)
 */
void handleStartCommand(const CanOtaStartCommand* cmd, uint8_t sourceCanId);

/**
 * @brief Handle OTA DATA chunk (slave side)
 */
void handleDataChunk(const uint8_t* data, size_t len, uint8_t sourceCanId);

/**
 * @brief Handle OTA VERIFY command (slave side)
 */
void handleVerifyCommand(const CanOtaVerifyCommand* cmd, uint8_t sourceCanId);

/**
 * @brief Handle OTA FINISH command (slave side)
 */
void handleFinishCommand(uint8_t sourceCanId);

/**
 * @brief Handle OTA ABORT command (slave side)
 */
void handleAbortCommand(uint8_t sourceCanId);

/**
 * @brief Handle STATUS query (slave side)
 */
void handleStatusQuery(uint8_t sourceCanId);

/**
 * @brief Send positive acknowledgment (slave side)
 */
void sendAck(uint8_t status, uint8_t sourceCanId);

/**
 * @brief Send negative acknowledgment (slave side)
 */
void sendNak(uint8_t status, uint16_t errorChunk, uint32_t expectedCrc, uint32_t receivedCrc, uint8_t sourceCanId);

/**
 * @brief Send status response (slave side)
 */
void sendStatusResponse(uint8_t sourceCanId);

/**
 * @brief Abort OTA and clean up (slave side)
 */
void abortOta(uint8_t status);

/**
 * @brief Loop function for timeout handling (slave side)
 * Call this from main loop
 */
void loop();

/**
 * @brief Get current OTA context state
 */
CanOtaState getState();

/**
 * @brief Get current progress percentage
 */
uint8_t getProgress();

// ============================================================================
// Master-side relay functions
// ============================================================================

#ifdef CAN_SEND_COMMANDS
/**
 * @brief Relay OTA start command from serial to CAN slave (master side)
 */
int relayStartToSlave(uint8_t slaveId, uint32_t firmwareSize, uint32_t totalChunks, 
                      uint16_t chunkSize, const uint8_t* md5Hash);

/**
 * @brief Relay OTA data chunk from serial to CAN slave (master side)
 */
int relayDataToSlave(uint8_t slaveId, uint16_t chunkIndex, uint16_t chunkSize, 
                     uint32_t crc32, const uint8_t* data);

/**
 * @brief Relay OTA verify command from serial to CAN slave (master side)
 */
int relayVerifyToSlave(uint8_t slaveId, const uint8_t* md5Hash);

/**
 * @brief Relay OTA finish command from serial to CAN slave (master side)
 */
int relayFinishToSlave(uint8_t slaveId);

/**
 * @brief Relay OTA abort command from serial to CAN slave (master side)
 */
int relayAbortToSlave(uint8_t slaveId);

/**
 * @brief Relay OTA status query from serial to CAN slave (master side)
 */
int relayStatusQueryToSlave(uint8_t slaveId);

/**
 * @brief Handle incoming ACK/NAK from slave and send response to serial (master side)
 */
void handleSlaveResponse(uint8_t messageType, const uint8_t* data, size_t len);
#endif

/**
 * @brief Process CAN OTA JSON command (master side - from serial)
 * @param doc JSON document containing the OTA command
 * @return qid or error code
 */
int actFromJson(cJSON* doc);

} // namespace can_ota
