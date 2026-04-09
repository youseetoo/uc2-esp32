// DeviceRouter.h — Serial-to-CANopen bridge for the master node.
// Routes parsed JSON commands to CAN slave nodes via SDO client.
// Only active on master builds (CAN_CONTROLLER_CANOPEN + CAN_SEND_COMMANDS).
#pragma once
#if defined(CAN_CONTROLLER_CANOPEN) && defined(CAN_SEND_COMMANDS)

#include <cJSON.h>
#include <stdint.h>

class DeviceRouter {
public:
    // Route a JSON command to the appropriate CAN slave via SDO.
    // Returns the JSON response (caller must free with cJSON_Delete),
    // or nullptr if this task is not handled by CAN routing.
    static cJSON* routeCommand(const char* task, cJSON* doc);

private:
    // Map stepperid to CANopen node-id
    // stepperid 0=A->node14, 1=X->node11, 2=Y->node12, 3=Z->node13
    static uint8_t stepperIdToNodeId(int stepperid);
    // Map stepperid to axis index on its node (0-3)
    // Currently always 0 since each node has one motor driver
    static uint8_t stepperIdToAxisOnNode(int stepperid);

    // Command handlers
    static cJSON* handleMotorAct(cJSON* doc);
    static cJSON* handleMotorGet(cJSON* doc);
    static cJSON* handleLaserAct(cJSON* doc);
    static cJSON* handleHomeAct(cJSON* doc);
    static cJSON* handleStateGet(uint8_t nodeId);
};

#endif // CAN_CONTROLLER_CANOPEN && CAN_SEND_COMMANDS
