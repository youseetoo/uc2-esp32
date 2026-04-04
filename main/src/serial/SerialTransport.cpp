#include "SerialTransport.h"
#include "COBS.h"
#include "Arduino.h"
#include "esp_log.h"
#include "../config/RuntimeConfig.h"

#if defined(CAN_BUS_ENABLED) && !defined(UC2_CANOPEN_ENABLED)
#include "../can/BinaryOtaProtocol.h"
#include "../can/CanOtaStreaming.h"
#endif

static const char *TAG = "SerialTransport";

// Legacy JSON buffer is static (matches the old serialInputBuffer size).
// COBS buffers are heap-allocated in setup() to avoid DRAM BSS overflow on
// ESP32 targets with many features enabled — heap has far more headroom than BSS.
#define LEGACY_BUF_SIZE  8192
#define COBS_BUF_SIZE    4096
#define COBS_MAX_ENCODED (COBS_BUF_SIZE + COBS_BUF_SIZE / 254 + 2)

namespace SerialTransport
{
    // ── Legacy JSON accumulator (brace-counting) — static ────────────
    static char legacyBuf[LEGACY_BUF_SIZE];
    static size_t legacyPos = 0;
    static bool inJsonObject = false;
    static int braceCount = 0;

    // ── COBS frame accumulator — heap allocated ───────────────────────
    static uint8_t *cobsBuf = nullptr;
    static size_t cobsPos = 0;
    static bool inCobsFrame = false;

    // ── Decoded payload scratch — heap allocated ──────────────────────
    static uint8_t *decodeBuf = nullptr;

    // ── Output buffer — heap allocated ───────────────────────────────
    static uint8_t *encodeBuf = nullptr;

    void setup()
    {
        legacyPos = 0;
        inJsonObject = false;
        braceCount = 0;
        cobsPos = 0;
        inCobsFrame = false;

        // Allocate COBS buffers from heap (only once)
        if (cobsBuf == nullptr)
            cobsBuf = (uint8_t *)malloc(COBS_BUF_SIZE);
        if (decodeBuf == nullptr)
            decodeBuf = (uint8_t *)malloc(COBS_BUF_SIZE);
        if (encodeBuf == nullptr)
            encodeBuf = (uint8_t *)malloc(COBS_MAX_ENCODED);
    }

    bool isInBinaryMode()
    {
#if defined(CAN_BUS_ENABLED) && !defined(UC2_CANOPEN_ENABLED)
        if (binary_ota::isInBinaryMode())
            return true;
        if (can_ota_stream::isStreamingModeActive())
            return true;
#endif
        return false;
    }

    // Try to parse a legacy (newline-delimited, brace-counted) JSON frame
    static cJSON *tryParseLegacy(uint8_t c)
    {
        if (c == '{')
        {
            if (!inJsonObject)
            {
                inJsonObject = true;
                legacyPos = 0;
                braceCount = 0;
            }
            braceCount++;
        }

        if (inJsonObject)
        {
            if (legacyPos < sizeof(legacyBuf) - 1)
            {
                legacyBuf[legacyPos++] = (char)c;
            }
            else
            {
                // overflow
                log_e("Legacy input buffer overflow");
                inJsonObject = false;
                legacyPos = 0;
                braceCount = 0;
                return NULL;
            }

            if (c == '}')
            {
                braceCount--;
                if (braceCount == 0)
                {
                    legacyBuf[legacyPos] = '\0';
                    cJSON *doc = cJSON_Parse(legacyBuf);
                    inJsonObject = false;
                    legacyPos = 0;
                    return doc; // may be NULL on parse error
                }
            }
        }
        return NULL;
    }

    // Process a complete COBS frame (between 0x00 delimiters)
    static cJSON *processCobsFrame(TransportFormat &outFormat)
    {
        if (cobsPos == 0 || cobsBuf == nullptr || decodeBuf == nullptr)
            return NULL;

        // First byte after delimiter is the header byte
        uint8_t header = cobsBuf[0];
        uint8_t *cobsData = cobsBuf + 1;
        size_t cobsLen = cobsPos - 1;

        if (cobsLen == 0)
            return NULL;

        size_t decoded = COBS::decode(cobsData, cobsLen, decodeBuf);
        if (decoded == 0)
        {
            log_w("COBS decode failed");
            return NULL;
        }

        if (header == SERIAL_HDR_JSON)
        {
            outFormat = TransportFormat::COBS_JSON;
            decodeBuf[decoded] = '\0';
            return cJSON_Parse((const char *)decodeBuf);
        }
        else if (header == SERIAL_HDR_MSGPACK)
        {
            // MsgPack decode — placeholder for future implementation
            outFormat = TransportFormat::COBS_MSGPACK;
            log_w("MsgPack decoding not yet implemented");
            return NULL;
        }
        else
        {
            log_w("Unknown COBS header byte: 0x%02x", header);
            return NULL;
        }
    }

    cJSON *readNext(TransportFormat &outFormat)
    {
        outFormat = TransportFormat::LEGACY_JSON;

        while (Serial.available() > 0)
        {
            uint8_t c = Serial.read();

            // 0x00 is the COBS frame delimiter
            if (c == 0x00)
            {
                if (inCobsFrame && cobsPos > 0)
                {
                    // End of a COBS frame
                    cJSON *result = processCobsFrame(outFormat);
                    cobsPos = 0;
                    inCobsFrame = false;
                    if (result != NULL)
                        return result;
                }
                else
                {
                    // Start of a new COBS frame
                    inCobsFrame = true;
                    cobsPos = 0;
                }
                continue;
            }

            if (inCobsFrame)
            {
                // Accumulate COBS data
                if (cobsBuf != nullptr && cobsPos < COBS_BUF_SIZE)
                {
                    cobsBuf[cobsPos++] = c;
                }
                else if (cobsBuf == nullptr)
                {
                    // Heap allocation failed — fall through to legacy mode
                    inCobsFrame = false;
                }
                else
                {
                    log_e("COBS buffer overflow");
                    inCobsFrame = false;
                    cobsPos = 0;
                }
            }
            else
            {
                // Legacy mode: brace-counted JSON
                cJSON *result = tryParseLegacy(c);
                if (result != NULL)
                {
                    outFormat = TransportFormat::LEGACY_JSON;
                    return result;
                }
            }
        }
        return NULL;
    }

    void sendJSON(cJSON *doc, TransportFormat format)
    {
        if (doc == NULL)
            return;

        char *jsonStr = cJSON_PrintUnformatted(doc);
        if (jsonStr == NULL)
            return;

        sendJSONString(jsonStr, format);
        free(jsonStr);
    }

    void sendJSONString(const char *jsonStr, TransportFormat format)
    {
        if (jsonStr == NULL)
            return;

        size_t jsonLen = strlen(jsonStr);

        if ((format == TransportFormat::COBS_JSON || format == TransportFormat::COBS_MSGPACK)
            && encodeBuf != nullptr)
        {
            // COBS framed output: 0x00 | header | COBS(json) | 0x00
            uint8_t header = SERIAL_HDR_JSON;

            // Encode payload with COBS
            size_t cobsLen = COBS::encode((const uint8_t *)jsonStr, jsonLen, encodeBuf);

            // Send frame: delimiter + header + COBS data + delimiter
            uint8_t delim = 0x00;
            Serial.write(&delim, 1);
            Serial.write(&header, 1);
            Serial.write(encodeBuf, cobsLen);
            Serial.write(&delim, 1);
            Serial.flush();
        }
        else
        {
            // Legacy format: ++\n<json>\n--\n
            Serial.print("++\n");
            Serial.print(jsonStr);
            Serial.print("\n--\n");
            Serial.flush();
        }
    }
}
