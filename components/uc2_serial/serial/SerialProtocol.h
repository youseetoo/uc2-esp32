#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

// Message format identifiers. The values ARE the on-wire magic bytes
// (kept identical to the original `enum class MessageFormat : uint8_t`).
typedef enum MessageFormat {
    JSON    = 0x4A,   // 'J'
    MSGPACK = 0x4D,   // 'M'
    BINARY  = 0x42,   // 'B' (reserved for future use)
    UNKNOWN = 0x00
} MessageFormat;

// Maximum message payload size in bytes.
#define SERIAL_PROTOCOL_MAX_MESSAGE_SIZE 8192

/**
 * @brief Detect message format from first bytes.
 * @param data Pointer to message data.
 * @param length Length of data.
 * @return Detected MessageFormat (UNKNOWN if not recognized).
 */
MessageFormat SerialProtocol_detectFormat(const uint8_t *data, size_t length);

/**
 * @brief Get message info without full parsing.
 * @param data Pointer to message data.
 * @param length Length of data.
 * @param format Output: detected format.
 * @param payloadLen Output: payload length.
 * @return true if the message header is valid.
 */
bool SerialProtocol_getMessageInfo(const uint8_t *data, size_t length,
                                   MessageFormat *format, uint16_t *payloadLen);

/**
 * @brief Check if a complete message is present.
 * @param data Pointer to message data.
 * @param length Length of data.
 * @return true if the message is complete and can be decoded.
 */
bool SerialProtocol_isMessageComplete(const uint8_t *data, size_t length);

/**
 * @brief Decode a received message to a cJSON object.
 * @param data Pointer to message data.
 * @param length Length of data.
 * @return Decoded cJSON object (caller must free) or NULL on error.
 */
cJSON *SerialProtocol_decode(const uint8_t *data, size_t length);

/**
 * @brief Encode a cJSON object to bytes for transmission.
 * @param doc cJSON object to encode.
 * @param format Desired output format.
 * @param outLen Output: encoded length.
 * @return Pointer to encoded data (caller must free) or NULL on error.
 */
uint8_t *SerialProtocol_encode(cJSON *doc, MessageFormat format, size_t *outLen);

/**
 * @brief Build a 4-byte header for the given format/payload length
 *        (exposed for unit tests of the private buildHeader helper).
 */
void SerialProtocol_buildHeaderTest(uint8_t *header, MessageFormat format,
                                    uint16_t payloadLen);

#ifdef __cplusplus
}
#endif

#endif // SERIAL_PROTOCOL_H
