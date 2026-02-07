/**
 * @file BinaryOtaProtocol.cpp
 * @brief Implementation of Binary OTA Protocol for high-speed firmware updates
 */

#include <PinConfig.h>


#include "BinaryOtaProtocol.h"
#include "CanOtaHandler.h"
#include "CanOtaTypes.h"
#include <esp_crc.h>
#include <esp_log.h>
#include <Arduino.h>

namespace binary_ota {

// State variables
static bool binaryModeActive = false;
static uint8_t targetCanId = 0;
static uint16_t expectedChunk = 0;
static uint32_t lastPacketTime = 0;

// Receive buffer
static uint8_t rxBuffer[BINARY_OTA_MAX_PACKET_SIZE];
static size_t rxPos = 0;
static bool synced = false;

// Statistics
static uint32_t packetsReceived = 0;
static uint32_t packetsRelayed = 0;
static uint32_t errors = 0;

uint32_t calculateCrc32(const uint8_t* data, size_t len) {
    return esp_crc32_le(0, data, len);
}

void sendAck(uint8_t status, uint16_t chunkIndex) {
    BinaryOtaResponse resp;
    resp.sync1 = BINARY_OTA_SYNC_1;
    resp.sync2 = BINARY_OTA_SYNC_2;
    resp.cmd = BINARY_OTA_CMD_ACK;
    resp.status = status;
    resp.chunkIndex = __builtin_bswap16(chunkIndex);  // Convert to big endian
    
    // Calculate checksum over everything except checksum byte
    resp.checksum = calculateChecksum((uint8_t*)&resp, sizeof(resp) - 1);
    
    Serial.write((uint8_t*)&resp, sizeof(resp));
    Serial.flush();
}

void sendNak(uint8_t errorCode, uint16_t expectedChunk) {
    BinaryOtaResponse resp;
    resp.sync1 = BINARY_OTA_SYNC_1;
    resp.sync2 = BINARY_OTA_SYNC_2;
    resp.cmd = BINARY_OTA_CMD_NAK;
    resp.status = errorCode;
    resp.chunkIndex = __builtin_bswap16(expectedChunk);  // Convert to big endian
    
    resp.checksum = calculateChecksum((uint8_t*)&resp, sizeof(resp) - 1);
    
    Serial.write((uint8_t*)&resp, sizeof(resp));
    Serial.flush();
}

bool isInBinaryMode() {
    return binaryModeActive;
}

// Store original log level to restore later
static esp_log_level_t originalLogLevel = ESP_LOG_INFO;

bool enterBinaryMode(uint8_t canId) {
    if (binaryModeActive) {
        log_w("Already in binary OTA mode");
        return false;
    }
    
    log_i("Entering binary OTA mode for CAN ID 0x%02X", canId);
    log_i("Switching to %d baud", BINARY_OTA_HIGH_BAUDRATE);
    
    // CRITICAL: Disable Serial logging during binary mode!
    // Log messages would interfere with binary protocol
    Serial.flush();
    delay(10);
    originalLogLevel = esp_log_level_get("*");
    esp_log_level_set("*", ESP_LOG_NONE);  // Disable all logging
    
    // Store target
    targetCanId = canId;
    expectedChunk = 0;
    packetsReceived = 0;
    packetsRelayed = 0;
    errors = 0;
    
    // Flush serial buffers - make sure all pending data is sent
    /*
    Serial.flush();
    delay(10);
    while (Serial.available()) Serial.read();

    // Switch to high baudrate
    Serial.end();
    delay(50);
    Serial.begin(BINARY_OTA_HIGH_BAUDRATE);
    Serial.setTimeout(BINARY_OTA_PACKET_TIMEOUT_MS);
    Serial.println("I'm in high baudrate now");
    // CRITICAL: Wait for Python to also switch baudrate
    // Python needs time to close port, open at new baudrate
    // This delay allows for synchronization
    delay(500);  // 500ms should be enough for Python to switch
    // Clear any garbage that might have accumulated
    while (Serial.available()) Serial.read();
    */
    
    // Clear state
    rxPos = 0;
    synced = false;
    binaryModeActive = true;
    lastPacketTime = millis();
    
    // Send ready signal (simple ACK with status OK and chunk 0)
    // Now Python should be listening at 2Mbaud
    sendAck(BINARY_OTA_OK, 0);
    log_i("Sent initial ACK");
    
    // Send it again after a short delay for reliability
    delay(100);
    sendAck(BINARY_OTA_OK, 0);
    log_i("Sent second ACK for reliability");
    
    log_i("Binary OTA mode active at %d baud", BINARY_OTA_HIGH_BAUDRATE);
    return true;
}

void exitBinaryMode() {
    if (!binaryModeActive) {
        return;
    }
    
    // Restore logging before printing stats
    esp_log_level_set("*", originalLogLevel);
    
    log_i("Exiting binary OTA mode. Stats: received=%lu, relayed=%lu, errors=%lu",
          packetsReceived, packetsRelayed, errors);
    
    /*
    // Flush and switch back to normal baudrate
    Serial.flush();
    delay(10);
    while (Serial.available()) Serial.read();
    
    Serial.end();
    delay(50);
    Serial.begin(BINARY_OTA_NORMAL_BAUDRATE);
    Serial.setTimeout(100);
    delay(50);
    */
    
    binaryModeActive = false;
    targetCanId = 0;
    expectedChunk = 0;
    rxPos = 0;
    synced = false;
    
    log_i("Binary OTA mode exited, back to %d baud", BINARY_OTA_NORMAL_BAUDRATE);
}

uint8_t getTargetCanId() {
    return targetCanId;
}

/**
 * @brief Process a complete binary packet
 */
static bool processCompletePacket() {
    // Validate minimum size
    log_i("Processing complete packet of size %d", rxPos);
    if (rxPos < BINARY_OTA_HEADER_SIZE + 1) {  // +1 for checksum
        log_e("Packet too small: %d bytes", rxPos);
        errors++;
        return false;
    }
    
    // Parse header
    BinaryOtaHeader* header = (BinaryOtaHeader*)rxBuffer;
    
    // Convert from big endian
    uint16_t chunkIndex = __builtin_bswap16(header->chunkIndex);
    uint16_t dataSize = __builtin_bswap16(header->dataSize);
    uint32_t expectedCrc = header->crc32;  // Already little endian
    
    // Validate data size
    if (dataSize > BINARY_OTA_MAX_CHUNK_SIZE) {
        log_e("Data size too large: %d", dataSize);
        sendNak(BINARY_OTA_ERR_SIZE, expectedChunk);
        errors++;
        return false;
    }
    
    // Calculate expected packet size
    size_t expectedSize = BINARY_OTA_HEADER_SIZE + dataSize + 1;  // +1 for checksum
    if (rxPos != expectedSize) {
        log_e("Packet size mismatch: got %d, expected %d", rxPos, expectedSize);
        sendNak(BINARY_OTA_ERR_SIZE, expectedChunk);
        errors++;
        return false;
    }
    
    // Verify checksum (XOR of all bytes except checksum itself)
    uint8_t packetChecksum = rxBuffer[rxPos - 1];
    uint8_t calculatedChecksum = calculateChecksum(rxBuffer, rxPos - 1);
    if (packetChecksum != calculatedChecksum) {
        log_e("Checksum mismatch: got 0x%02X, expected 0x%02X", packetChecksum, calculatedChecksum);
        sendNak(BINARY_OTA_ERR_CHECKSUM, expectedChunk);
        errors++;
        return false;
    }
    
    // Handle command
    switch (header->cmd) {
        case BINARY_OTA_CMD_DATA: {
            // Get data pointer
            uint8_t* data = rxBuffer + BINARY_OTA_HEADER_SIZE;
            
            // Verify CRC32 of data
            uint32_t calculatedCrc = calculateCrc32(data, dataSize);
            if (calculatedCrc != expectedCrc) {
                log_e("CRC32 mismatch for chunk %d: got 0x%08X, expected 0x%08X", 
                      chunkIndex, calculatedCrc, expectedCrc);
                sendNak(BINARY_OTA_ERR_CRC, expectedChunk);
                errors++;
                return false;
            }
            
            // Check sequence
            if (chunkIndex != expectedChunk) {
                log_w("Sequence error: expected %d, got %d", expectedChunk, chunkIndex);
                sendNak(BINARY_OTA_ERR_SEQUENCE, expectedChunk);
                errors++;
                return false;
            }
            
            // Relay to CAN slave
            #ifdef CAN_SEND_COMMANDS
            int result = can_ota::relayDataToSlave(targetCanId, chunkIndex, dataSize, expectedCrc, data);
            if (result < 0) {
                log_e("Failed to relay chunk %d to CAN slave", chunkIndex);
                sendNak(BINARY_OTA_ERR_RELAY, expectedChunk);
                errors++;
                return false;
            }
            packetsRelayed++;
            #endif
            
            // Success - send ACK and increment expected chunk
            packetsReceived++;
            expectedChunk++;
            lastPacketTime = millis();
            
            // Only log every 100 chunks to reduce overhead
            if (chunkIndex % 100 == 0) {
                log_i("Chunk %d relayed successfully", chunkIndex);
            }
            
            sendAck(BINARY_OTA_OK, chunkIndex);
            return true;
        }
        
        case BINARY_OTA_CMD_END: {
            log_i("Binary OTA END received after %d chunks", expectedChunk);
            sendAck(BINARY_OTA_OK, expectedChunk);
            // Don't exit binary mode here - let the Python side do it
            return true;
        }
        
        case BINARY_OTA_CMD_ABORT: {
            log_w("Binary OTA ABORT received");
            sendAck(BINARY_OTA_OK, expectedChunk);
            exitBinaryMode();
            return true;
        }
        
        case BINARY_OTA_CMD_SWITCH_BAUD: {
            // Switch back to normal baudrate
            log_i("Baudrate switch command received");
            sendAck(BINARY_OTA_OK, expectedChunk);
            Serial.flush();
            delay(50);
            exitBinaryMode();
            return true;
        }
        
        default:
            log_e("Unknown command: 0x%02X", header->cmd);
            sendNak(BINARY_OTA_ERR_SYNC, expectedChunk);
            errors++;
            return false;
    }
}

bool processBinaryPacket() {
    if (!binaryModeActive) {
        return false;
    }
    
    // Check for timeout
    if (millis() - lastPacketTime > BINARY_OTA_SYNC_TIMEOUT_MS) {
        log_w("Binary OTA timeout - no data for %d ms", BINARY_OTA_SYNC_TIMEOUT_MS);
        exitBinaryMode();
        return false;
    }
    
    // Read available bytes
    while (Serial.available() > 0) {
        uint8_t byte = Serial.read();
        lastPacketTime = millis();
        // log_i("Received byte: 0x%02X", byte);
        // Look for sync sequence
        // log_i("RX pos: %d, header size: %d, synced: %d, byte: 0x%02X", rxPos, BINARY_OTA_HEADER_SIZE, synced, byte);
        if (!synced) {
            if (rxPos == 0) {
                if (byte == BINARY_OTA_SYNC_1) {
                    rxBuffer[rxPos++] = byte;
                    //log_i("Sync byte 1 detected");
                }
                else{
                    
                    //log_i("Waiting for sync byte 1, got 0x%02X", byte);
                }
            } else if (rxPos == 1) {
                if (byte == BINARY_OTA_SYNC_2) {
                    rxBuffer[rxPos++] = byte;
                    synced = true;
                    // log_i("Sync byte 2 detected, synchronized");
                } else if (byte == BINARY_OTA_SYNC_1) {
                    // Stay at position 1
                    // log_i("Received sync byte 1 again at position 1");
                } else {
                    rxPos = 0;  // Reset
                    // log_i("Sync byte 2 mismatch, resetting sync");
                }
            }
            continue;
        }
        
        // Store byte
        if (rxPos < sizeof(rxBuffer)) {
            rxBuffer[rxPos++] = byte;
        } else {
            // Buffer overflow - reset
            log_e("RX buffer overflow");
            rxPos = 0;
            synced = false;
            errors++;
            continue;
        }
        
        // Check if we have the header
        if (rxPos >= BINARY_OTA_HEADER_SIZE) {
            //log_i("Header received, checking for complete packet");
            BinaryOtaHeader* header = (BinaryOtaHeader*)rxBuffer;
            uint16_t dataSize = __builtin_bswap16(header->dataSize);
            size_t expectedSize = BINARY_OTA_HEADER_SIZE + dataSize + 1;  // +1 for checksum
            
            // Check if packet is complete
            if (rxPos >= expectedSize) {
                bool result = processCompletePacket();
                //log_i("Complete packet received of size %d, result: %d", rxPos, result);
                
                // Reset for next packet
                rxPos = 0;
                synced = false;
                
                return result;
            }
        }
    }
    
    return false;
}

} // namespace binary_ota

