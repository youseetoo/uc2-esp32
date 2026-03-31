/**
 * SlaveController.cpp
 *
 * UC2 CAN Slave — device state management and OD callbacks.
 *
 * Device driver calls are guarded by compile flags so the same file
 * can be built for motor-only, laser-only, or multi-device slaves:
 *   -DMOTOR_CONTROLLER   enables FocusMotor calls
 *   -DLASER_CONTROLLER   enables LaserController calls
 *   -DLED_CONTROLLER     enables LedController calls
 */
#include "SlaveController.h"
#include "CANopen/CanOpenStack.h"
#include "CANopen/ObjectDictionary.h"
#include <string.h>
#include <Arduino.h>
#include "esp_log.h"

#ifdef MOTOR_CONTROLLER
#include "motor/FocusMotor.h"
#include "motor/MotorTypes.h"
#endif
#ifdef LASER_CONTROLLER
#include "laser/LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "led/LedController.h"
#endif

#define TAG_SC "UC2_SC"

namespace SlaveController {

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
static uint8_t     s_caps     = 0;
static uint8_t     s_nodeId   = 0;
static uint8_t     s_numMotors = 0;
static uint8_t     s_numLasers = 0;
static bool        s_hasLED   = false;
static uint8_t     s_errReg   = 0;

static MotorState  s_motors[SLAVE_MAX_MOTORS] = {};
static LaserState  s_lasers[SLAVE_MAX_LASERS] = {};
static LEDState    s_led                      = {};

// Track whether motor was running last loop to detect stop events
static bool        s_wasRunning[SLAVE_MAX_MOTORS] = {};
static uint32_t    s_lastStatusMs = 0;

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void init(uint8_t caps, uint8_t nodeId,
          uint8_t motors, uint8_t lasers, bool hasLED)
{
    s_caps     = caps;
    s_nodeId   = nodeId;
    s_numMotors = (motors < SLAVE_MAX_MOTORS) ? motors : SLAVE_MAX_MOTORS;
    s_numLasers = (lasers < SLAVE_MAX_LASERS) ? lasers : SLAVE_MAX_LASERS;
    s_hasLED   = hasLED;
    s_errReg   = 0;

    memset(s_motors,     0, sizeof(s_motors));
    memset(s_lasers,     0, sizeof(s_lasers));
    memset(s_wasRunning, 0, sizeof(s_wasRunning));
    memset(&s_led,       0, sizeof(s_led));

    s_lastStatusMs = 0;

    ESP_LOGI(TAG_SC, "SlaveController init: nodeId=0x%02X caps=0x%02X motors=%d lasers=%d led=%d",
             nodeId, caps, motors, lasers, (int)hasLED);
}

// ---------------------------------------------------------------------------
// loop — called every iteration from main.cpp
// ---------------------------------------------------------------------------
void loop()
{
    uint32_t now = (uint32_t)millis();

    // Update actual motor positions and detect move completions
    for (uint8_t a = 0; a < s_numMotors; a++) {
#ifdef MOTOR_CONTROLLER
        s_motors[a].actualPos  = FocusMotor::getCurrentMotorPosition(a);
        s_motors[a].isRunning  = FocusMotor::isRunning(a);
#endif
        // Publish TPDO1 when motor transitions from running → stopped
        bool running = s_motors[a].isRunning;
        if (s_wasRunning[a] && !running) {
            CanOpenStack::publishMotorStatus(a, s_motors[a].actualPos,
                                            false, s_motors[a].qid);
            ESP_LOGD(TAG_SC, "Motor %d stopped at %ld", a, (long)s_motors[a].actualPos);
        }
        s_wasRunning[a] = running;
    }

    // Publish TPDO2 (node status) once per second
    if (now - s_lastStatusMs >= 1000u) {
        s_lastStatusMs = now;
        CanOpenStack::publishNodeStatus(s_caps, s_errReg, 0, 0);
    }
}

// ---------------------------------------------------------------------------
// SDO read callback — master reads a value from us
// ---------------------------------------------------------------------------
bool sdoRead(uint16_t index, uint8_t subIndex,
             uint8_t* buf, uint8_t* lenOut, void* /*ctx*/)
{
    // Node capabilities
    if (index == UC2_OD_NODE_CAPS && subIndex == 0x00) {
        buf[0] = s_caps;
        *lenOut = 1;
        return true;
    }

    // Motor axis objects (0x2101 + axis)
    if (index >= UC2_OD_MOTOR_BASE &&
        index <  UC2_OD_MOTOR_BASE + SLAVE_MAX_MOTORS) {
        uint8_t axis = (uint8_t)(index - UC2_OD_MOTOR_BASE);
        if (axis >= s_numMotors) return false;

#ifdef MOTOR_CONTROLLER
        s_motors[axis].actualPos = FocusMotor::getCurrentMotorPosition(axis);
        s_motors[axis].isRunning = FocusMotor::isRunning(axis);
#endif
        switch (subIndex) {
        case UC2_OD_MOTOR_SUB_TARGET:
            memcpy(buf, &s_motors[axis].targetPos, 4); *lenOut = 4; return true;
        case UC2_OD_MOTOR_SUB_SPEED:
            memcpy(buf, &s_motors[axis].speed,     4); *lenOut = 4; return true;
        case UC2_OD_MOTOR_SUB_CMD:
            buf[0] = s_motors[axis].cmd;               *lenOut = 1; return true;
        case UC2_OD_MOTOR_SUB_ACTUAL:
            memcpy(buf, &s_motors[axis].actualPos, 4); *lenOut = 4; return true;
        case UC2_OD_MOTOR_SUB_RUNNING:
            buf[0] = s_motors[axis].isRunning ? 1u : 0u; *lenOut = 1; return true;
        case UC2_OD_MOTOR_SUB_QID:
            memcpy(buf, &s_motors[axis].qid, 2);       *lenOut = 2; return true;
        default: return false;
        }
    }

    // Laser channel objects (0x2201 + channel)
    if (index >= UC2_OD_LASER_BASE &&
        index <  UC2_OD_LASER_BASE + SLAVE_MAX_LASERS) {
        uint8_t ch = (uint8_t)(index - UC2_OD_LASER_BASE);
        if (ch >= s_numLasers) return false;
        if (subIndex == UC2_OD_LASER_SUB_PWM) {
            memcpy(buf, &s_lasers[ch].pwm, 2); *lenOut = 2; return true;
        }
        return false;
    }

    // LED state
    if (index == UC2_OD_LED_CTRL) {
        if (!s_hasLED) return false;
        switch (subIndex) {
        case UC2_OD_LED_SUB_MODE: buf[0] = s_led.mode; *lenOut = 1; return true;
        case UC2_OD_LED_SUB_R:    buf[0] = s_led.r;    *lenOut = 1; return true;
        case UC2_OD_LED_SUB_G:    buf[0] = s_led.g;    *lenOut = 1; return true;
        case UC2_OD_LED_SUB_B:    buf[0] = s_led.b;    *lenOut = 1; return true;
        case UC2_OD_LED_SUB_IDX:
            memcpy(buf, &s_led.ledIndex, 2); *lenOut = 2; return true;
        default: return false;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// SDO write callback — master writes a value to us
// ---------------------------------------------------------------------------
bool sdoWrite(uint16_t index, uint8_t subIndex,
              const uint8_t* data, uint8_t dataLen, void* /*ctx*/)
{
    // Motor axis objects (0x2101 + axis)
    if (index >= UC2_OD_MOTOR_BASE &&
        index <  UC2_OD_MOTOR_BASE + SLAVE_MAX_MOTORS) {
        uint8_t axis = (uint8_t)(index - UC2_OD_MOTOR_BASE);
        if (axis >= s_numMotors || dataLen == 0) return false;

        switch (subIndex) {
        case UC2_OD_MOTOR_SUB_TARGET:
            if (dataLen < 4) return false;
            memcpy(&s_motors[axis].targetPos, data, 4);
            return true;
        case UC2_OD_MOTOR_SUB_SPEED:
            if (dataLen < 4) return false;
            memcpy(&s_motors[axis].speed, data, 4);
            return true;
        case UC2_OD_MOTOR_SUB_CMD: {
            // Writing CMD actually triggers the motor action
            s_motors[axis].cmd = data[0];
#ifdef MOTOR_CONTROLLER
            MotorData* md = FocusMotor::getData()[axis];
            switch (data[0]) {
            case UC2_MOTOR_CMD_STOP:
                FocusMotor::stopStepper(axis);
                break;
            case UC2_MOTOR_CMD_MOVE_REL:
                md->targetPosition   = s_motors[axis].targetPos;
                md->speed            = s_motors[axis].speed;
                md->absolutePosition = false;
                md->isforever        = false;
                md->isStop           = false;
                FocusMotor::startStepper(axis, 0);
                break;
            case UC2_MOTOR_CMD_MOVE_ABS:
                md->targetPosition   = s_motors[axis].targetPos;
                md->speed            = s_motors[axis].speed;
                md->absolutePosition = true;
                md->isforever        = false;
                md->isStop           = false;
                FocusMotor::startStepper(axis, 0);
                break;
            case UC2_MOTOR_CMD_HOME:
                FocusMotor::stopStepper(axis);
                break;
            }
#endif
            return true;
        }
        case UC2_OD_MOTOR_SUB_QID:
            if (dataLen < 2) return false;
            memcpy(&s_motors[axis].qid, data, 2);
            return true;
        default:
            return false;
        }
    }

    // Laser channel objects (0x2201 + channel)
    if (index >= UC2_OD_LASER_BASE &&
        index <  UC2_OD_LASER_BASE + SLAVE_MAX_LASERS) {
        uint8_t ch = (uint8_t)(index - UC2_OD_LASER_BASE);
        if (ch >= s_numLasers) return false;
        if (subIndex == UC2_OD_LASER_SUB_PWM && dataLen >= 2) {
            memcpy(&s_lasers[ch].pwm, data, 2);
#ifdef LASER_CONTROLLER
            LaserController::setLaserVal(ch, (int)s_lasers[ch].pwm, 0, 0, 0);
#endif
            return true;
        }
        return false;
    }

    // Heartbeat producer time (allow master to configure it)
    if (index == UC2_OD_HEARTBEAT_TIME && subIndex == 0x00 && dataLen >= 2) {
        uint16_t periodMs = 0;
        memcpy(&periodMs, data, 2);
        ESP_LOGI(TAG_SC, "SDO: set heartbeat period to %u ms", periodMs);
        // CanOpenStack will re-init HeartbeatManager if needed
        return true;
    }

    // LED state
    if (index == UC2_OD_LED_CTRL) {
        if (!s_hasLED) return false;
        bool apply = false;
        switch (subIndex) {
        case UC2_OD_LED_SUB_MODE: s_led.mode = data[0]; apply = true; break;
        case UC2_OD_LED_SUB_R:    s_led.r    = data[0]; apply = true; break;
        case UC2_OD_LED_SUB_G:    s_led.g    = data[0]; apply = true; break;
        case UC2_OD_LED_SUB_B:    s_led.b    = data[0]; apply = true; break;
        case UC2_OD_LED_SUB_IDX:
            if (dataLen >= 2) { memcpy(&s_led.ledIndex, data, 2); apply = true; }
            break;
        default: return false;
        }
        if (apply) {
#ifdef LED_CONTROLLER
            LedCommand cmd = {};
            cmd.mode     = (LedMode)s_led.mode;
            cmd.r        = s_led.r;
            cmd.g        = s_led.g;
            cmd.b        = s_led.b;
            cmd.ledIndex = s_led.ledIndex;
            LedController::act(&cmd);
#endif
        }
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// RPDO callback — called from CAN RX task for every addressed PDO
// ---------------------------------------------------------------------------
void onRPDO(const UC2_RPDO1_MotorPos* rp1,
            const UC2_RPDO2_Control*  rp2,
            const UC2_RPDO3_LEDExtra* rp3)
{
    // RPDO1: motor position + speed (does NOT trigger move — wait for RPDO2 cmd)
    if (rp1) {
        // Default to axis 0; axis is specified in RPDO2
        // Store for the subsequent RPDO2 with the axis + command
        s_motors[0].targetPos = rp1->target_pos;
        s_motors[0].speed     = rp1->speed;
    }

    // RPDO2: control frame — may carry motor cmd, laser, or LED mode byte
    if (rp2) {
        if (rp2->axis < s_numMotors) {
            // Motor command
            uint8_t axis = rp2->axis;
            s_motors[axis].cmd = rp2->cmd;

            // If RPDO1 target was meant for a different axis, fix it up now
            s_motors[axis].targetPos = s_motors[0].targetPos;
            s_motors[axis].speed     = s_motors[0].speed;

#ifdef MOTOR_CONTROLLER
            MotorData* md = FocusMotor::getData()[axis];
            switch (rp2->cmd) {
            case UC2_MOTOR_CMD_STOP:
                FocusMotor::stopStepper(axis);
                break;
            case UC2_MOTOR_CMD_MOVE_REL:
                md->targetPosition   = s_motors[axis].targetPos;
                md->speed            = s_motors[axis].speed;
                md->absolutePosition = false;
                md->isforever        = false;
                md->isStop           = false;
                FocusMotor::startStepper(axis, 0);
                ESP_LOGD(TAG_SC, "RPDO: motor %d REL %ld @ %ld",
                         axis, (long)md->targetPosition, (long)md->speed);
                break;
            case UC2_MOTOR_CMD_MOVE_ABS:
                md->targetPosition   = s_motors[axis].targetPos;
                md->speed            = s_motors[axis].speed;
                md->absolutePosition = true;
                md->isforever        = false;
                md->isStop           = false;
                FocusMotor::startStepper(axis, 0);
                break;
            case UC2_MOTOR_CMD_HOME:
                FocusMotor::stopStepper(axis);
                break;
            }
#endif
        }

        // Laser command (when axis == 0xFF, treat as laser-only)
        if (rp2->laser_id < s_numLasers) {
            s_lasers[rp2->laser_id].pwm = rp2->laser_pwm;
#ifdef LASER_CONTROLLER
            LaserController::setLaserVal(rp2->laser_id, (int)rp2->laser_pwm, 0, 0, 0);
#endif
        }

        // LED RGB (partial — blue arrives in RPDO3)
        if (s_hasLED) {
            s_led.mode = rp2->led_mode;
            s_led.r    = rp2->r;
            s_led.g    = rp2->g;
        }
    }

    // RPDO3: LED blue + led_index + qid
    if (rp3 && s_hasLED) {
        int16_t qid = (int16_t)((uint16_t)rp3->qid_lo | ((uint16_t)rp3->qid_hi << 8u));
        s_led.b        = rp3->b;
        s_led.ledIndex = rp3->led_index;

        // Now we have the full LED command — apply it
#ifdef LED_CONTROLLER
        LedCommand cmd = {};
        cmd.mode     = (LedMode)s_led.mode;
        cmd.r        = s_led.r;
        cmd.g        = s_led.g;
        cmd.b        = s_led.b;
        cmd.ledIndex = s_led.ledIndex;
        cmd.qid      = qid;
        LedController::act(&cmd);
#endif
        ESP_LOGD(TAG_SC, "RPDO: LED mode=%d rgb=(%d,%d,%d) idx=%d",
                 s_led.mode, s_led.r, s_led.g, s_led.b, s_led.ledIndex);
    }
}

// ---------------------------------------------------------------------------
// State accessors
// ---------------------------------------------------------------------------
const MotorState* getMotor(uint8_t axis)
{
    if (axis >= SLAVE_MAX_MOTORS) return nullptr;
    return &s_motors[axis];
}
const LaserState* getLaser(uint8_t channel)
{
    if (channel >= SLAVE_MAX_LASERS) return nullptr;
    return &s_lasers[channel];
}
const LEDState* getLED()        { return &s_led; }
uint8_t getCaps()               { return s_caps; }
uint8_t getNodeId()             { return s_nodeId; }
uint8_t getErrorReg()           { return s_errReg; }

} // namespace SlaveController
