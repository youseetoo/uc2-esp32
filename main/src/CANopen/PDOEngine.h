/**
 * PDOEngine.h
 *
 * UC2 CANopen-Lite PDO encoder/decoder.
 *
 * This module handles fixed-mapping PDOs for all UC2 device types.
 * It does NOT require the CANopenNode library — it works directly with
 * raw TWAI frames using standard CANopen CAN-ID allocation.
 *
 * Master sends RPDOs to slaves; slaves send TPDOs to master.
 * All mappings are fixed (not runtime-configurable) for simplicity.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ObjectDictionary.h"
#include "driver/twai.h"

namespace PDOEngine {

// Initialize PDO engine (no-op for fixed mappings — TWAI managed by CanOpenStack)
void init();

// ============================================================================
// Encoding helpers — master encodes these before transmitting to a slave
// ============================================================================

// RPDO1: motor position + speed command (triggers move when UC2_MOTOR_CMD set via RPDO2)
twai_message_t encodeMotorPosPDO(uint8_t nodeId, int32_t targetPos, int32_t speed);

// RPDO2: combined motor-trigger / laser / LED control frame
// axis=0xFF → laser command only; axis=0xFE → LED command only
twai_message_t encodeControlPDO(uint8_t nodeId,
                                uint8_t axis,    uint8_t motorCmd,
                                uint8_t laserId, uint16_t laserPwm,
                                uint8_t ledMode, uint8_t r, uint8_t g);

// RPDO3: LED extended (blue channel + pixel index + QID)
twai_message_t encodeLEDExtraPDO(uint8_t nodeId,
                                  uint8_t b, uint16_t ledIndex, int16_t qid);

// ============================================================================
// Encoding helpers — slave encodes these before transmitting to master
// ============================================================================

// TPDO1: motor status — sent by slave whenever axis state changes
twai_message_t encodeMotorStatusPDO(uint8_t nodeId,
                                    uint8_t axis, int32_t actualPos,
                                    bool isRunning, int16_t qid);

// TPDO2: node status — sent by slave periodically (~1 s)
twai_message_t encodeNodeStatusPDO(uint8_t nodeId,
                                   uint8_t caps, uint8_t errReg,
                                   int16_t tempX10, int16_t encoderPos,
                                   uint8_t nmtState);

// TPDO3: extended sensor data
twai_message_t encodeSensorDataPDO(uint8_t nodeId,
                                   int32_t encoderPos32, uint16_t adcRaw,
                                   uint8_t dinState, uint8_t doutState);

// ============================================================================
// Decoding helpers — used on both sides to parse incoming frames
// ============================================================================

// Returns true if the frame is a RPDO1 addressed to nodeId
bool decodeMotorPosPDO(const twai_message_t& frame, uint8_t nodeId,
                       UC2_RPDO1_MotorPos* out);

// Returns true if the frame is a RPDO2 addressed to nodeId
bool decodeControlPDO(const twai_message_t& frame, uint8_t nodeId,
                      UC2_RPDO2_Control* out);

// Returns true if the frame is a RPDO3 addressed to nodeId
bool decodeLEDExtraPDO(const twai_message_t& frame, uint8_t nodeId,
                       UC2_RPDO3_LEDExtra* out);

// Returns true if the frame is a TPDO1 from any slave; fills nodeIdOut
bool decodeMotorStatusPDO(const twai_message_t& frame,
                          uint8_t* nodeIdOut, UC2_TPDO1_MotorStatus* out);

// Returns true if the frame is a TPDO2 from any slave; fills nodeIdOut
bool decodeNodeStatusPDO(const twai_message_t& frame,
                         uint8_t* nodeIdOut, UC2_TPDO2_NodeStatus* out);

// Returns true if the frame is a TPDO3 from any slave; fills nodeIdOut
bool decodeSensorDataPDO(const twai_message_t& frame,
                         uint8_t* nodeIdOut, UC2_TPDO3_SensorData* out);

// ============================================================================
// Frame classification helpers
// ============================================================================

// True if this frame is a UC2 PDO (not SDO, NMT, or heartbeat)
bool isUC2PDO(const twai_message_t& frame);

// True if this is any TPDO from the given node (master uses this to filter)
bool isTpdoFromNode(const twai_message_t& frame, uint8_t nodeId);

// True if this is a heartbeat frame
bool isHeartbeat(const twai_message_t& frame);

// True if this is an NMT frame
bool isNMT(const twai_message_t& frame);

// True if this is an SDO frame (request or response)
bool isSDO(const twai_message_t& frame);

} // namespace PDOEngine
