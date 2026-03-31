/**
 * SDOHandler.cpp
 *
 * UC2 CANopen-Lite SDO implementation.
 * Implements expedited SDO client (master) and SDO server (slave).
 * Uses raw TWAI — no external CANopen library dependency.
 */
#include "SDOHandler.h"
#include "ObjectDictionary.h"
#include <string.h>
#include <Arduino.h>
#include "esp_log.h"

#define TAG_SDO "UC2_SDO"

// CiA 301 SDO command specifiers
#define SDO_CS_WRITE_4B   0x23u
#define SDO_CS_WRITE_3B   0x27u
#define SDO_CS_WRITE_2B   0x2Bu
#define SDO_CS_WRITE_1B   0x2Fu
#define SDO_CS_WRITE_RESP 0x60u
#define SDO_CS_READ_REQ   0x40u
#define SDO_CS_READ_RESP  0x40u  // 0x4x, x encodes valid bytes
#define SDO_CS_ABORT      0x80u

// Common SDO abort codes (CiA 301 Table 20)
#define SDO_ABORT_NOT_EXIST   0x06020000ul  // object does not exist
#define SDO_ABORT_NO_SUB      0x06090011ul  // sub-index does not exist
#define SDO_ABORT_WO          0x06010001ul  // write-only — cannot read
#define SDO_ABORT_RO          0x06010002ul  // read-only  — cannot write
#define SDO_ABORT_GENERAL     0x08000000ul

namespace SDOHandler {

// ===========================================================================
// SDO CLIENT — master sends requests, waits for slave response
// ===========================================================================

// Build the command specifier byte for an expedited write (n = data length 1-4)
static uint8_t write_cs(uint8_t n)
{
    // cs = 0x20 | (e=1 << 1) | (s=1 << 0) | ((4-n) << 2)
    return (uint8_t)(0x23u | (uint8_t)((4u - n) << 2));
}

// Build a read (upload) request frame
static twai_message_t make_read_req(uint8_t nodeId, uint16_t index, uint8_t subIndex)
{
    twai_message_t f = {};
    f.identifier       = UC2_COBID_SDO_REQ_BASE + nodeId;
    f.extd             = 0;
    f.rtr              = 0;
    f.data_length_code = 8;
    f.data[0] = SDO_CS_READ_REQ;
    f.data[1] = (uint8_t)(index & 0xFFu);
    f.data[2] = (uint8_t)(index >> 8);
    f.data[3] = subIndex;
    return f;
}

// Build a write (download) request frame
static twai_message_t make_write_req(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                                     const void* data, uint8_t len)
{
    twai_message_t f = {};
    f.identifier       = UC2_COBID_SDO_REQ_BASE + nodeId;
    f.extd             = 0;
    f.rtr              = 0;
    f.data_length_code = 8;
    f.data[0] = write_cs(len);
    f.data[1] = (uint8_t)(index & 0xFFu);
    f.data[2] = (uint8_t)(index >> 8);
    f.data[3] = subIndex;
    memcpy(&f.data[4], data, len);
    return f;
}

bool waitSDOResponse(uint8_t nodeId, twai_message_t* out, uint32_t timeoutMs)
{
    uint32_t deadline = (uint32_t)millis() + timeoutMs;
    while ((uint32_t)millis() < deadline) {
        twai_message_t rx;
        // Poll TWAI receive queue with a short timeout
        esp_err_t err = twai_receive(&rx, pdMS_TO_TICKS(10));
        if (err != ESP_OK) continue;
        // Accept only SDO responses addressed to us from this nodeId
        if (rx.identifier == UC2_COBID_SDO_RESP_BASE + nodeId) {
            *out = rx;
            return true;
        }
        // Discard other frames (not ideal — CanOpenStack owns the RX queue;
        // this helper is for use when the SDO is called synchronously outside
        // of the main CAN task, e.g. during boot enumeration)
    }
    return false;
}

bool writeExpedited(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                    const void* data, uint8_t dataLen)
{
    if (dataLen == 0 || dataLen > 4) return false;

    for (uint8_t attempt = 0; attempt < SDO_MAX_RETRIES; attempt++) {
        twai_message_t req = make_write_req(nodeId, index, subIndex, data, dataLen);
        if (twai_transmit(&req, pdMS_TO_TICKS(20)) != ESP_OK) {
            ESP_LOGW(TAG_SDO, "TX failed attempt %d", attempt);
            continue;
        }
        twai_message_t resp;
        if (!waitSDOResponse(nodeId, &resp, SDO_TIMEOUT_MS)) {
            ESP_LOGW(TAG_SDO, "SDO write timeout (node=%d idx=0x%04X sub=%d attempt=%d)",
                     nodeId, index, subIndex, attempt);
            continue;
        }
        if ((resp.data[0] & 0xE0u) == SDO_CS_ABORT) {
            uint32_t ac = (uint32_t)resp.data[4]       |
                          ((uint32_t)resp.data[5] << 8) |
                          ((uint32_t)resp.data[6] << 16)|
                          ((uint32_t)resp.data[7] << 24);
            ESP_LOGW(TAG_SDO, "SDO write abort 0x%08lX (node=%d idx=0x%04X)",
                     (unsigned long)ac, nodeId, index);
            return false;
        }
        if (resp.data[0] == SDO_CS_WRITE_RESP) {
            return true;
        }
    }
    return false;
}

bool readExpedited(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                   void* buf, uint8_t bufLen, uint8_t* bytesRead)
{
    if (bufLen == 0 || bufLen > 4) return false;

    for (uint8_t attempt = 0; attempt < SDO_MAX_RETRIES; attempt++) {
        twai_message_t req = make_read_req(nodeId, index, subIndex);
        if (twai_transmit(&req, pdMS_TO_TICKS(20)) != ESP_OK) {
            ESP_LOGW(TAG_SDO, "TX failed attempt %d", attempt);
            continue;
        }
        twai_message_t resp;
        if (!waitSDOResponse(nodeId, &resp, SDO_TIMEOUT_MS)) {
            ESP_LOGW(TAG_SDO, "SDO read timeout (node=%d idx=0x%04X sub=%d)",
                     nodeId, index, subIndex);
            continue;
        }
        if ((resp.data[0] & 0xE0u) == SDO_CS_ABORT) {
            uint32_t ac = (uint32_t)resp.data[4]       |
                          ((uint32_t)resp.data[5] << 8) |
                          ((uint32_t)resp.data[6] << 16)|
                          ((uint32_t)resp.data[7] << 24);
            ESP_LOGW(TAG_SDO, "SDO read abort 0x%08lX (node=%d idx=0x%04X)",
                     (unsigned long)ac, nodeId, index);
            return false;
        }
        // Response cs = 0x4x: bits 2-3 encode (4 - n), x = 4-n<<2 | e<<1 | s
        uint8_t n = 4u - ((resp.data[0] >> 2u) & 0x03u);
        if (n > 4) n = 4;
        n = (n < bufLen) ? n : bufLen;
        memcpy(buf, &resp.data[4], n);
        if (bytesRead) *bytesRead = n;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Typed convenience wrappers
// ---------------------------------------------------------------------------

bool writeU8(uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t v)
    { return writeExpedited(nodeId, idx, sub, &v, 1); }
bool writeU16(uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t v)
    { return writeExpedited(nodeId, idx, sub, &v, 2); }
bool writeU32(uint8_t nodeId, uint16_t idx, uint8_t sub, uint32_t v)
    { return writeExpedited(nodeId, idx, sub, &v, 4); }
bool writeI32(uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t v)
    { return writeExpedited(nodeId, idx, sub, &v, 4); }
bool readU8(uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t* out)
    { uint8_t n = 0; return readExpedited(nodeId, idx, sub, out, 1, &n); }
bool readU16(uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t* out)
    { uint8_t n = 0; return readExpedited(nodeId, idx, sub, out, 2, &n); }
bool readI32(uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t* out)
    { uint8_t n = 0; return readExpedited(nodeId, idx, sub, out, 4, &n); }

// ---------------------------------------------------------------------------
// UC2 convenience SDO commands
// ---------------------------------------------------------------------------

bool motorCommand(uint8_t nodeId, uint8_t axis,
                  int32_t targetPos, int32_t speed, uint8_t cmd)
{
    uint16_t idx = (uint16_t)(UC2_OD_MOTOR_BASE + axis);
    bool ok = true;
    ok &= writeI32(nodeId, idx, UC2_OD_MOTOR_SUB_TARGET, targetPos);
    ok &= writeI32(nodeId, idx, UC2_OD_MOTOR_SUB_SPEED,  speed);
    ok &= writeU8 (nodeId, idx, UC2_OD_MOTOR_SUB_CMD,    cmd);
    return ok;
}

bool laserSet(uint8_t nodeId, uint8_t channel, uint16_t pwm)
{
    uint16_t idx = (uint16_t)(UC2_OD_LASER_BASE + channel);
    return writeU16(nodeId, idx, UC2_OD_LASER_SUB_PWM, pwm);
}

bool ledSet(uint8_t nodeId, uint8_t mode,
            uint8_t r, uint8_t g, uint8_t b, uint16_t ledIndex)
{
    bool ok = true;
    ok &= writeU8 (nodeId, UC2_OD_LED_CTRL, UC2_OD_LED_SUB_MODE, mode);
    ok &= writeU8 (nodeId, UC2_OD_LED_CTRL, UC2_OD_LED_SUB_R,    r);
    ok &= writeU8 (nodeId, UC2_OD_LED_CTRL, UC2_OD_LED_SUB_G,    g);
    ok &= writeU8 (nodeId, UC2_OD_LED_CTRL, UC2_OD_LED_SUB_B,    b);
    ok &= writeU16(nodeId, UC2_OD_LED_CTRL, UC2_OD_LED_SUB_IDX,  ledIndex);
    return ok;
}

bool motorGetPos(uint8_t nodeId, uint8_t axis, int32_t* posOut)
{
    uint16_t idx = (uint16_t)(UC2_OD_MOTOR_BASE + axis);
    return readI32(nodeId, idx, UC2_OD_MOTOR_SUB_ACTUAL, posOut);
}

bool motorIsRunning(uint8_t nodeId, uint8_t axis, bool* runningOut)
{
    uint8_t v = 0;
    uint16_t idx = (uint16_t)(UC2_OD_MOTOR_BASE + axis);
    if (!readU8(nodeId, idx, UC2_OD_MOTOR_SUB_RUNNING, &v)) return false;
    *runningOut = (v != 0);
    return true;
}

bool nodeGetCaps(uint8_t nodeId, uint8_t* capsOut)
{
    return readU8(nodeId, UC2_OD_NODE_CAPS, 0x00, capsOut);
}

bool configureHeartbeat(uint8_t nodeId, uint16_t periodMs)
{
    return writeU16(nodeId, UC2_OD_HEARTBEAT_TIME, 0x00, periodMs);
}

void sendAbort(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t abortCode)
{
    twai_message_t f = {};
    f.identifier       = UC2_COBID_SDO_RESP_BASE + nodeId;
    f.extd             = 0;
    f.rtr              = 0;
    f.data_length_code = 8;
    f.data[0] = SDO_CS_ABORT;
    f.data[1] = (uint8_t)(index & 0xFFu);
    f.data[2] = (uint8_t)(index >> 8);
    f.data[3] = subIndex;
    f.data[4] = (uint8_t)(abortCode & 0xFFu);
    f.data[5] = (uint8_t)((abortCode >> 8 ) & 0xFFu);
    f.data[6] = (uint8_t)((abortCode >> 16) & 0xFFu);
    f.data[7] = (uint8_t)((abortCode >> 24) & 0xFFu);
    twai_transmit(&f, pdMS_TO_TICKS(10));
}

// ===========================================================================
// SDO SERVER — slave processes incoming SDO requests from master
// ===========================================================================

static uint8_t     s_ownNodeId = 0;
static OD_ReadCb   s_readCb    = nullptr;
static OD_WriteCb  s_writeCb   = nullptr;
static void*       s_ctx       = nullptr;

void sdoServerInit(uint8_t ownNodeId, OD_ReadCb readCb, OD_WriteCb writeCb, void* ctx)
{
    s_ownNodeId = ownNodeId;
    s_readCb    = readCb;
    s_writeCb   = writeCb;
    s_ctx       = ctx;
}

bool sdoServerProcessFrame(const twai_message_t& frame)
{
    // Only act on SDO request frames addressed to own node
    if (frame.identifier != UC2_COBID_SDO_REQ_BASE + s_ownNodeId) return false;
    if (frame.data_length_code < 4) return false;

    uint8_t  cs      = frame.data[0];
    uint16_t index   = (uint16_t)(frame.data[1] | ((uint16_t)frame.data[2] << 8));
    uint8_t  subIdx  = frame.data[3];

    if ((cs & 0xF0u) == 0x40u) {
        // ---- Upload (read) request ----
        if (!s_readCb) {
            sendAbort(s_ownNodeId, index, subIdx, SDO_ABORT_GENERAL);
            return true;
        }
        uint8_t buf[4] = {};
        uint8_t len = 0;
        if (!s_readCb(index, subIdx, buf, &len, s_ctx)) {
            sendAbort(s_ownNodeId, index, subIdx, SDO_ABORT_NOT_EXIST);
            return true;
        }
        // Build expedited response
        twai_message_t resp = {};
        resp.identifier       = UC2_COBID_SDO_RESP_BASE + s_ownNodeId;
        resp.extd             = 0;
        resp.rtr              = 0;
        resp.data_length_code = 8;
        // cs = 0x40 | (e=1<<1) | (s=1<<0) | ((4-len)<<2)
        resp.data[0] = (uint8_t)(0x43u | (uint8_t)((4u - (len & 3u)) << 2));
        resp.data[1] = frame.data[1];  // index lo
        resp.data[2] = frame.data[2];  // index hi
        resp.data[3] = subIdx;
        memcpy(&resp.data[4], buf, (len < 4 ? len : 4));
        twai_transmit(&resp, pdMS_TO_TICKS(20));
        return true;
    }

    if ((cs & 0xE0u) == 0x20u) {
        // ---- Download (write) request ----
        // Number of data bytes: s=bit0 set means n=(4-e_size), e=bit1
        uint8_t n = 4u - ((cs >> 2u) & 0x03u);
        if (n > 4) n = 4;
        if (!s_writeCb) {
            sendAbort(s_ownNodeId, index, subIdx, SDO_ABORT_GENERAL);
            return true;
        }
        if (!s_writeCb(index, subIdx, &frame.data[4], n, s_ctx)) {
            sendAbort(s_ownNodeId, index, subIdx, SDO_ABORT_NOT_EXIST);
            return true;
        }
        // Send write acknowledgement
        twai_message_t resp = {};
        resp.identifier       = UC2_COBID_SDO_RESP_BASE + s_ownNodeId;
        resp.extd             = 0;
        resp.rtr              = 0;
        resp.data_length_code = 8;
        resp.data[0] = SDO_CS_WRITE_RESP;
        resp.data[1] = frame.data[1];
        resp.data[2] = frame.data[2];
        resp.data[3] = subIdx;
        twai_transmit(&resp, pdMS_TO_TICKS(20));
        return true;
    }

    // Unrecognised command specifier
    sendAbort(s_ownNodeId, index, subIdx, SDO_ABORT_GENERAL);
    return true;
}

} // namespace SDOHandler
