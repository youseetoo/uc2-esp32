/**
 * @file BinaryOtaProtocol.h
 * @brief Binary OTA Protocol for high-speed firmware updates over serial
 * 
 * This protocol bypasses JSON for data transfer, using raw binary packets
 * for maximum throughput. The flow is:
 * 
 * 1. JSON START command initiates binary mode and high baudrate
 * 2. Binary DATA packets sent at high speed (2Mbaud)
 * 3. Binary ACK/NAK responses for flow control
 * 4. Switch back to normal baudrate after transfer
 * 5. JSON END/VERIFY for finalization
 * 
 * Binary Packet Format:
 *   [SYNC_1][SYNC_2][CMD][CHUNK_H][CHUNK_L][SIZE_H][SIZE_L][CRC32_LE(4)][DATA...][CHECKSUM]
 *   
 *   - SYNC_1, SYNC_2: 0xAA, 0x55 (magic bytes)
 *   - CMD: Command type (0x01=DATA, 0x06=ACK, 0x07=NAK, 0x0F=END)
 *   - CHUNK_H/L: 16-bit chunk index (big endian)
 *   - SIZE_H/L: 16-bit data size (big endian)
 *   - CRC32: 32-bit CRC of DATA only (little endian)
 *   - DATA: Raw firmware bytes
 *   - CHECKSUM: XOR of all bytes from SYNC_1 to last DATA byte
 * 
 * ACK Response Format (sent by Master after relaying to Slave):
 *   [0xAA][0x55][0x06][STATUS][CHUNK_H][CHUNK_L][CHECKSUM]
 * 
 * NAK Response Format:
 *   [0xAA][0x55][0x07][ERROR_CODE][EXPECTED_CHUNK_H][EXPECTED_CHUNK_L][CHECKSUM]
 */

#ifndef BINARY_OTA_PROTOCOL_H
#define BINARY_OTA_PROTOCOL_H

#include <Arduino.h>
#include <cstdint>

// Protocol constants
#define BINARY_OTA_SYNC_1           0xAA
#define BINARY_OTA_SYNC_2           0x55

// Command types
#define BINARY_OTA_CMD_DATA         0x01
#define BINARY_OTA_CMD_ACK          0x06
#define BINARY_OTA_CMD_NAK          0x07
#define BINARY_OTA_CMD_END          0x0F
#define BINARY_OTA_CMD_ABORT        0x10
#define BINARY_OTA_CMD_SWITCH_BAUD  0x11  // Command to switch baudrate

// Baudrates
#define BINARY_OTA_HIGH_BAUDRATE    2000000  // 2 Mbaud for data transfer
#define BINARY_OTA_NORMAL_BAUDRATE  115200   // Normal baudrate

// Timeouts
#define BINARY_OTA_PACKET_TIMEOUT_MS  100   // Timeout for receiving complete packet
#define BINARY_OTA_SYNC_TIMEOUT_MS    5000  // Timeout waiting for first sync byte

// Buffer sizes
#define BINARY_OTA_MAX_CHUNK_SIZE     1024  // Max chunk size
#define BINARY_OTA_HEADER_SIZE        11    // SYNC(2) + CMD(1) + CHUNK(2) + SIZE(2) + CRC32(4)
#define BINARY_OTA_MAX_PACKET_SIZE    (BINARY_OTA_HEADER_SIZE + BINARY_OTA_MAX_CHUNK_SIZE + 1)

// Status codes (same as CANOtaStatus)
enum BinaryOtaStatus {
    BINARY_OTA_OK               = 0x00,
    BINARY_OTA_ERR_SYNC         = 0x01,
    BINARY_OTA_ERR_CHECKSUM     = 0x02,
    BINARY_OTA_ERR_CRC          = 0x03,
    BINARY_OTA_ERR_SEQUENCE     = 0x04,
    BINARY_OTA_ERR_SIZE         = 0x05,
    BINARY_OTA_ERR_TIMEOUT      = 0x06,
    BINARY_OTA_ERR_RELAY        = 0x07,  // Failed to relay to CAN slave
    BINARY_OTA_ERR_SLAVE_NAK    = 0x08,  // Slave sent NAK
};

/**
 * @brief Binary OTA packet header structure
 */
struct __attribute__((packed)) BinaryOtaHeader {
    uint8_t sync1;      // Must be 0xAA
    uint8_t sync2;      // Must be 0x55
    uint8_t cmd;        // Command type
    uint16_t chunkIndex; // Chunk index (big endian in wire format)
    uint16_t dataSize;   // Data size (big endian in wire format)
    uint32_t crc32;      // CRC32 of data (little endian)
};

/**
 * @brief Binary OTA ACK/NAK response structure
 */
struct __attribute__((packed)) BinaryOtaResponse {
    uint8_t sync1;      // 0xAA
    uint8_t sync2;      // 0x55
    uint8_t cmd;        // ACK=0x06, NAK=0x07
    uint8_t status;     // Status code
    uint16_t chunkIndex; // Received/expected chunk index
    uint8_t checksum;   // XOR checksum
};

namespace binary_ota {

/**
 * @brief Calculate XOR checksum of data
 */
inline uint8_t calculateChecksum(const uint8_t* data, size_t len) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief Calculate CRC32 (same as zlib)
 */
uint32_t calculateCrc32(const uint8_t* data, size_t len);

/**
 * @brief Send ACK response
 */
void sendAck(uint8_t status, uint16_t chunkIndex);

/**
 * @brief Send NAK response
 */
void sendNak(uint8_t errorCode, uint16_t expectedChunk);

/**
 * @brief Check if we're currently in binary OTA mode
 */
bool isInBinaryMode();

/**
 * @brief Enter binary OTA mode with high baudrate
 * @param targetCanId CAN ID of slave device
 * @return true if successful
 */
bool enterBinaryMode(uint8_t targetCanId);

/**
 * @brief Exit binary OTA mode and restore normal baudrate
 */
void exitBinaryMode();

/**
 * @brief Process binary OTA data from serial
 * Called from main loop when in binary mode
 * @return true if packet was processed
 */
bool processBinaryPacket();

/**
 * @brief Get current target CAN ID
 */
uint8_t getTargetCanId();

} // namespace binary_ota

#endif // BINARY_OTA_PROTOCOL_H
