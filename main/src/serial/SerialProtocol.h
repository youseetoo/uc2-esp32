#ifndef SERIAL_PROTOCOL_H
#define SERIAL_PROTOCOL_H

#include <Arduino.h>
#include <cJSON.h>

enum class MessageFormat : uint8_t {
    JSON = 0x4A,      // 'J'
    MSGPACK = 0x4D,   // 'M'
    BINARY = 0x42,    // 'B' (reserved for future use)
    UNKNOWN = 0x00
};

/**
 * @brief Serial Protocol V2 - Handles MessagePack and JSON encoding/decoding
 * 
 * This class provides a unified interface for encoding/decoding messages in both
 * MessagePack (efficient) and JSON (legacy) formats. It maintains backward 
 * compatibility with the legacy delimiter-based JSON format.
 * 
 * Message Format:
 * - Header (4 bytes): [Magic][Length_L][Length_H][Reserved]
 *   - Magic: 0x4A (JSON) or 0x4D (MessagePack)
 *   - Length: 16-bit little-endian payload size
 *   - Reserved: 0x00 (for future use)
 * - Payload: JSON string or MessagePack binary data
 * 
 * Legacy Format:
 * - ++\n<JSON string>\n--\n
 */
class SerialProtocol {
public:
    static const uint8_t HEADER_SIZE = 4;
    static const uint16_t MAX_MESSAGE_SIZE = 8192;
    
    // Legacy delimiters
    static const char* LEGACY_START;  // "++\n"
    static const char* LEGACY_END;    // "\n--\n"
    static const size_t LEGACY_START_LEN = 3;
    static const size_t LEGACY_END_LEN = 4;
    
    /**
     * @brief Detect message format from first bytes
     * @param data Pointer to message data
     * @param len Length of data
     * @return Detected MessageFormat
     */
    static MessageFormat detectFormat(const uint8_t* data, size_t len);
    
    /**
     * @brief Decode received message to cJSON object
     * @param data Pointer to message data
     * @param len Length of data
     * @return Decoded cJSON object (caller must delete) or nullptr on error
     */
    static cJSON* decode(const uint8_t* data, size_t len);
    
    /**
     * @brief Encode cJSON object to bytes for transmission
     * @param doc cJSON object to encode
     * @param format Desired output format
     * @param outLen Output parameter for encoded length
     * @return Pointer to encoded data (caller must free) or nullptr on error
     */
    static uint8_t* encode(cJSON* doc, MessageFormat format, size_t& outLen);
    
    /**
     * @brief Get message info without full parsing
     * @param data Pointer to message data
     * @param len Length of data
     * @param format Output parameter for detected format
     * @param payloadLen Output parameter for payload length
     * @return true if message header is valid
     */
    static bool getMessageInfo(const uint8_t* data, size_t len, 
                              MessageFormat& format, uint16_t& payloadLen);
    
    /**
     * @brief Check if message is complete
     * @param data Pointer to message data
     * @param len Length of data
     * @return true if message is complete and can be decoded
     */
    static bool isMessageComplete(const uint8_t* data, size_t len);
    
private:
    // Format-specific decoders
    static cJSON* decodeMsgPack(const uint8_t* payload, size_t len);
    static cJSON* decodeJson(const uint8_t* payload, size_t len);
    static cJSON* decodeLegacy(const uint8_t* data, size_t len);
    
    // Format-specific encoders
    static uint8_t* encodeMsgPack(cJSON* doc, size_t& outLen);
    static uint8_t* encodeJson(cJSON* doc, size_t& outLen);
    static uint8_t* encodeLegacy(cJSON* doc, size_t& outLen);
    
    // Helper functions
    static void buildHeader(uint8_t* header, MessageFormat format, uint16_t payloadLen);
    static bool parseHeader(const uint8_t* header, MessageFormat& format, uint16_t& payloadLen);
    
    // MessagePack to cJSON conversion helpers
    #ifdef HAS_MPACK
    static cJSON* mpackNodeToJson(mpack_node_t node);
    static cJSON* mpackMapToJson(mpack_node_t node);
    static cJSON* mpackArrayToJson(mpack_node_t node);
    
    // cJSON to MessagePack conversion helpers
    static void jsonToMpack(mpack_writer_t* writer, cJSON* item);
    static void jsonObjectToMpack(mpack_writer_t* writer, cJSON* object);
    static void jsonArrayToMpack(mpack_writer_t* writer, cJSON* array);
    #endif
};

#endif // SERIAL_PROTOCOL_H
