#include <PinConfig.h>
#include "CANopenMotorBackend.h"

#ifdef CAN_CONTROLLER_CANOPEN
#include "../../canopen/CANopenModule.h"
#include "../../canopen/UC2_OD_Indices.h"
#include "esp_log.h"
#include <cstring>
#include <cstdlib>  // std::abs

static const char* TAG = "CANMotorBE";

CANopenMotorBackend::CANopenMotorBackend(const uint8_t nodeIds[4])
{
    memcpy(_nodeIds, nodeIds, sizeof(_nodeIds));
}

void CANopenMotorBackend::setup()
{
    ESP_LOGI(TAG, "CANopenMotorBackend: nodes [%d, %d, %d, %d]",
             _nodeIds[0], _nodeIds[1], _nodeIds[2], _nodeIds[3]);
}

void CANopenMotorBackend::loop()
{
    // Could poll position via SDO here; for now rely on cached values
}

bool CANopenMotorBackend::writeMotorSDO(int axis, MotorData* data, bool isStop)
{
    if (axis < 0 || axis > 3) return false;
    uint8_t nodeId = _nodeIds[axis];
    if (nodeId == 0) {
        ESP_LOGW(TAG, "No CAN node mapped for axis %d", axis);
        return false;
    }

    // Each slave currently hosts one motor; sub-index always 1 (1-based array).
    uint8_t sub = 1;

    if (!isStop && data != nullptr) {
        int32_t pos = data->targetPosition;
        CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_TARGET_POSITION, sub, (uint8_t*)&pos, 4);

        uint32_t speed = (uint32_t)std::abs(data->speed);
        CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_SPEED, sub, (uint8_t*)&speed, 4);

        if (data->acceleration > 0) {
            uint32_t accel = (uint32_t)data->acceleration;
            CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_ACCELERATION, sub, (uint8_t*)&accel, 4);
        }

        uint8_t isAbs = data->absolutePosition ? 1 : 0;
        CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_IS_ABSOLUTE, sub, &isAbs, 1);

        // Trigger start: bit[0] in command word
        uint8_t cmdWord = 0x01;
        return CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_COMMAND_WORD, 0x00, &cmdWord, 1);
    } else {
        // Trigger stop: bit[4] in command word
        uint8_t cmdWord = 0x10;
        return CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_COMMAND_WORD, 0x00, &cmdWord, 1);
    }
}

void CANopenMotorBackend::startMove(int axis, MotorData* data, int reduced)
{
    if (axis < 0 || axis > 3) return;
    bool ok = writeMotorSDO(axis, data, false);
    if (!ok) {
        ESP_LOGW(TAG, "startMove SDO failed for axis %d → node %d", axis, _nodeIds[axis]);
    } else {
        ESP_LOGI(TAG, "Motor cmd → node %d axis %d: pos=%ld speed=%ld",
                 _nodeIds[axis], axis, (long)data->targetPosition, (long)data->speed);
    }
}

void CANopenMotorBackend::stopMove(int axis)
{
    if (axis < 0 || axis > 3) return;
    writeMotorSDO(axis, nullptr, true);
}

long CANopenMotorBackend::getPosition(int axis) const
{
    if (axis < 0 || axis > 3) return 0;
    
    // Try SDO read for current position; fall back to cached value on failure
    uint8_t nodeId = _nodeIds[axis];
    if (nodeId == 0) return _cachedPos[axis];

    int32_t pos = 0;
    size_t readSize = 0;
    if (CANopenModule::readSDO(nodeId, UC2_OD::MOTOR_ACTUAL_POSITION, 1,
                               (uint8_t*)&pos, 4, &readSize)) {
        _cachedPos[axis] = pos;
    }
    return _cachedPos[axis];
}

bool CANopenMotorBackend::isRunning(int axis) const
{
    if (axis < 0 || axis > 3) return false;
    
    uint8_t nodeId = _nodeIds[axis];
    if (nodeId == 0) return false;

    uint8_t status = 0;
    size_t readSize = 0;
    if (CANopenModule::readSDO(nodeId, UC2_OD::MOTOR_STATUS_WORD, 1,
                               &status, 1, &readSize)) {
        _cachedRunning[axis] = (status & 0x01) != 0;
    }
    return _cachedRunning[axis];
}

void CANopenMotorBackend::updateData(int axis)
{
    // Force a position read from the slave
    if (axis >= 0 && axis <= 3) {
        getPosition(axis);
    }
}

void CANopenMotorBackend::setPosition(int axis, int pos)
{
    if (axis < 0 || axis > 3) return;
    _cachedPos[axis] = pos;
    // Optionally: SDO write to set actual position on slave
}

#endif // CAN_CONTROLLER_CANOPEN
