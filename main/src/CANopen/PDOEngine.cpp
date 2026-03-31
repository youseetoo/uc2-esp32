/**
 * PDOEngine.cpp
 *
 * UC2 CANopen-Lite PDO encoder/decoder implementation.
 * Works directly with raw TWAI frames — no external CANopen library needed.
 */
#include "PDOEngine.h"
#include <string.h>

namespace PDOEngine {

// ---------------------------------------------------------------------------
// Internal helper: build a standard 11-bit CAN data frame
// ---------------------------------------------------------------------------
static twai_message_t make_frame(uint32_t cobId, const void* data, uint8_t len)
{
    twai_message_t f = {};
    f.identifier       = cobId;
    f.extd             = 0;   // standard 11-bit frame
    f.rtr              = 0;
    f.data_length_code = len;
    if (data && len > 0) {
        memcpy(f.data, data, len);
    }
    return f;
}

void init() { /* nothing — fixed mappings need no init */ }

// ============================================================================
// Encoding — master side (RPDOs), slave side (TPDOs)
// ============================================================================

twai_message_t encodeMotorPosPDO(uint8_t nodeId, int32_t targetPos, int32_t speed)
{
    UC2_RPDO1_MotorPos p = { targetPos, speed };
    return make_frame(UC2_COBID_RPDO1_BASE + nodeId, &p, sizeof(p));
}

twai_message_t encodeControlPDO(uint8_t nodeId,
                                uint8_t axis,    uint8_t motorCmd,
                                uint8_t laserId, uint16_t laserPwm,
                                uint8_t ledMode, uint8_t r, uint8_t g)
{
    UC2_RPDO2_Control p = { axis, motorCmd, laserId, laserPwm, ledMode, r, g };
    return make_frame(UC2_COBID_RPDO2_BASE + nodeId, &p, sizeof(p));
}

twai_message_t encodeLEDExtraPDO(uint8_t nodeId,
                                  uint8_t b, uint16_t ledIndex, int16_t qid)
{
    UC2_RPDO3_LEDExtra p = {};
    p.b         = b;
    p.led_index = ledIndex;
    p.qid       = qid;
    return make_frame(UC2_COBID_RPDO3_BASE + nodeId, &p, sizeof(p));
}

twai_message_t encodeMotorStatusPDO(uint8_t nodeId,
                                    uint8_t axis, int32_t actualPos,
                                    bool isRunning, int16_t qid)
{
    UC2_TPDO1_MotorStatus p = { actualPos, axis, (uint8_t)isRunning, qid };
    return make_frame(UC2_COBID_TPDO1_BASE + nodeId, &p, sizeof(p));
}

twai_message_t encodeNodeStatusPDO(uint8_t nodeId,
                                   uint8_t caps, uint8_t errReg,
                                   int16_t tempX10, int16_t encoderPos,
                                   uint8_t nmtState)
{
    UC2_TPDO2_NodeStatus p = { caps, nodeId, errReg, tempX10, encoderPos, nmtState };
    return make_frame(UC2_COBID_TPDO2_BASE + nodeId, &p, sizeof(p));
}

twai_message_t encodeSensorDataPDO(uint8_t nodeId,
                                   int32_t encoderPos32, uint16_t adcRaw,
                                   uint8_t dinState, uint8_t doutState)
{
    UC2_TPDO3_SensorData p = { encoderPos32, adcRaw, dinState, doutState };
    return make_frame(UC2_COBID_TPDO3_BASE + nodeId, &p, sizeof(p));
}

// ============================================================================
// Decoding — parse incoming TWAI frames
// ============================================================================

bool decodeMotorPosPDO(const twai_message_t& frame, uint8_t nodeId,
                       UC2_RPDO1_MotorPos* out)
{
    if (frame.identifier != UC2_COBID_RPDO1_BASE + nodeId) return false;
    if (frame.data_length_code < sizeof(UC2_RPDO1_MotorPos)) return false;
    memcpy(out, frame.data, sizeof(UC2_RPDO1_MotorPos));
    return true;
}

bool decodeControlPDO(const twai_message_t& frame, uint8_t nodeId,
                      UC2_RPDO2_Control* out)
{
    if (frame.identifier != UC2_COBID_RPDO2_BASE + nodeId) return false;
    if (frame.data_length_code < sizeof(UC2_RPDO2_Control)) return false;
    memcpy(out, frame.data, sizeof(UC2_RPDO2_Control));
    return true;
}

bool decodeLEDExtraPDO(const twai_message_t& frame, uint8_t nodeId,
                       UC2_RPDO3_LEDExtra* out)
{
    if (frame.identifier != UC2_COBID_RPDO3_BASE + nodeId) return false;
    if (frame.data_length_code < 5) return false;  // b + led_index + qid minimum
    memcpy(out, frame.data, sizeof(UC2_RPDO3_LEDExtra));
    return true;
}

bool decodeMotorStatusPDO(const twai_message_t& frame,
                          uint8_t* nodeIdOut, UC2_TPDO1_MotorStatus* out)
{
    uint32_t id = frame.identifier;
    if (id <= UC2_COBID_TPDO1_BASE || id > UC2_COBID_TPDO1_BASE + 127u) return false;
    if (frame.data_length_code < sizeof(UC2_TPDO1_MotorStatus)) return false;
    *nodeIdOut = (uint8_t)(id - UC2_COBID_TPDO1_BASE);
    memcpy(out, frame.data, sizeof(UC2_TPDO1_MotorStatus));
    return true;
}

bool decodeNodeStatusPDO(const twai_message_t& frame,
                         uint8_t* nodeIdOut, UC2_TPDO2_NodeStatus* out)
{
    uint32_t id = frame.identifier;
    if (id <= UC2_COBID_TPDO2_BASE || id > UC2_COBID_TPDO2_BASE + 127u) return false;
    if (frame.data_length_code < sizeof(UC2_TPDO2_NodeStatus)) return false;
    *nodeIdOut = (uint8_t)(id - UC2_COBID_TPDO2_BASE);
    memcpy(out, frame.data, sizeof(UC2_TPDO2_NodeStatus));
    return true;
}

bool decodeSensorDataPDO(const twai_message_t& frame,
                         uint8_t* nodeIdOut, UC2_TPDO3_SensorData* out)
{
    uint32_t id = frame.identifier;
    if (id <= UC2_COBID_TPDO3_BASE || id > UC2_COBID_TPDO3_BASE + 127u) return false;
    if (frame.data_length_code < sizeof(UC2_TPDO3_SensorData)) return false;
    *nodeIdOut = (uint8_t)(id - UC2_COBID_TPDO3_BASE);
    memcpy(out, frame.data, sizeof(UC2_TPDO3_SensorData));
    return true;
}

// ============================================================================
// Frame classification
// ============================================================================

bool isUC2PDO(const twai_message_t& frame)
{
    uint32_t id = frame.identifier;
    // UC2 PDOs live in 0x181–0x4FF (TPDO1/2/3 and RPDO1/2/3)
    return (id >= 0x181u && id <= 0x4FFu);
}

bool isTpdoFromNode(const twai_message_t& frame, uint8_t nodeId)
{
    uint32_t id = frame.identifier;
    return (id == UC2_COBID_TPDO1_BASE + nodeId ||
            id == UC2_COBID_TPDO2_BASE + nodeId ||
            id == UC2_COBID_TPDO3_BASE + nodeId);
}

bool isHeartbeat(const twai_message_t& frame)
{
    uint32_t id = frame.identifier;
    return (id >= UC2_COBID_HEARTBEAT_BASE + 1u &&
            id <= UC2_COBID_HEARTBEAT_BASE + 127u);
}

bool isNMT(const twai_message_t& frame)
{
    return (frame.identifier == UC2_COBID_NMT);
}

bool isSDO(const twai_message_t& frame)
{
    uint32_t id = frame.identifier;
    return ((id >= UC2_COBID_SDO_RESP_BASE + 1u && id <= UC2_COBID_SDO_RESP_BASE + 127u) ||
            (id >= UC2_COBID_SDO_REQ_BASE  + 1u && id <= UC2_COBID_SDO_REQ_BASE  + 127u));
}

} // namespace PDOEngine
