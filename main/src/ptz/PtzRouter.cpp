// PtzRouter.cpp — PTZ-keyboard → CANopen SDO dispatcher.
//
// Mirrors JoystickRouter.cpp (the DS4 USB bridge): a 50 Hz task polls
// PtzKeyboard::getMotion(), issues the 6-frame REMOTE motor bursts against
// the routing table, and forwards discrete key events upstream via TPDO2.
// The SDO helpers (short timeout + dead-node muting) are copied from
// JoystickRouter so a missing slave can't stall this task either.
#include "PtzRouter.h"
#include "PinConfig.h"

#ifdef PTZ_KEYBOARD_CONTROLLER

#include <Arduino.h>
#include <cstdlib>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "PtzKeyboard.h"
#include "../canopen/CANopenModule.h"
#include "../canopen/RoutingTable.h"
#include "../canopen/UC2_OD_Indices.h"
#include "../motor/MotorTypes.h"

#ifdef CAN_CONTROLLER_CANOPEN
extern "C" {
#include <CANopen.h>
#include "OD.h"
}
#endif

static const char* TAG = "ptz_router";

namespace {

// Same top speed as the DS4 path (JoystickRouter kMaxSpeed) so the two
// input devices feel identical; per-axis multipliers come from PinConfig.
constexpr float kMaxSpeed      = 2000.0f;
constexpr float kOneOver63f    = 1.0f / 63.0f;

// Speed delta below which we don't re-issue a motor SDO burst.
constexpr int kSpeedHysteresis = 50;

constexpr uint32_t kSdoTimeoutMs        = 40;
constexpr uint8_t  kDeadNodeFailThreshold = 2;
constexpr uint32_t kDeadNodeBackoffMs     = 2000;

struct NodeHealth {
    uint8_t  nodeId   = 0;
    uint8_t  fails    = 0;
    uint32_t silentUntilMs = 0;
};
NodeHealth s_health[8];

static NodeHealth* healthSlot(uint8_t nodeId) {
    for (auto& h : s_health) if (h.nodeId == nodeId) return &h;
    for (auto& h : s_health) if (h.nodeId == 0)      { h.nodeId = nodeId; return &h; }
    return &s_health[0];
}

static inline bool nodeMuted(uint8_t nodeId) {
    return millis() < healthSlot(nodeId)->silentUntilMs;
}

static inline void noteSdo(uint8_t nodeId, bool ok) {
    NodeHealth* h = healthSlot(nodeId);
    if (ok) { h->fails = 0; h->silentUntilMs = 0; return; }
    if (++h->fails >= kDeadNodeFailThreshold) {
        h->silentUntilMs = millis() + kDeadNodeBackoffMs;
        log_w("node 0x%02X muted for %u ms (SDO timeouts)",
              nodeId, (unsigned)kDeadNodeBackoffMs);
    }
}

struct AxisRuntime {
    bool    running    = false;
    int32_t lastSpeed  = 0;
};
AxisRuntime s_axis[4]; // indexed by Stepper enum (A=0, X=1, Y=2, Z=3)

// Issue the 6-frame REMOTE motor command. Mirrors JoystickRouter.cpp /
// DeviceRouter.cpp:285-315.
static void emitMotorRemote(uint8_t nodeId, uint8_t subAxis,
                            int32_t pos, int32_t speed, uint32_t accel,
                            bool isAbs, bool isForever, bool isStop)
{
    if (nodeMuted(nodeId)) return;
    const uint8_t sub = subAxis + 1;
    bool ok = true;
    ok &= CANopenModule::writeSDO_i32(nodeId, UC2_OD::MOTOR_TARGET_POSITION, sub, pos, kSdoTimeoutMs);
    ok &= CANopenModule::writeSDO_u32(nodeId, UC2_OD::MOTOR_SPEED, sub, (uint32_t)speed, kSdoTimeoutMs);
    if (accel > 0)
        ok &= CANopenModule::writeSDO_u32(nodeId, UC2_OD::MOTOR_ACCELERATION, sub, accel, kSdoTimeoutMs);
    ok &= CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_IS_ABSOLUTE, sub, isAbs ? 1 : 0, kSdoTimeoutMs);
    ok &= CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_IS_FOREVER, sub, isForever ? 1 : 0, kSdoTimeoutMs);
    const uint8_t cmdWord = isStop
        ? (uint8_t)(1u << (subAxis + 4))
        : (uint8_t)(1u << subAxis);
    ok &= CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_COMMAND_WORD, 0x00, cmdWord, kSdoTimeoutMs);
    noteSdo(nodeId, ok);
}

static void stopAxis(int ax)
{
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, (uint8_t)ax);
    if (!route || route->where != UC2::RouteEntry::REMOTE) {
        s_axis[ax].running = false;
        return;
    }
    emitMotorRemote(route->nodeId, route->subAxis,
                    /*pos*/0, /*speed*/0, /*accel*/0,
                    /*isAbs*/false, /*isForever*/false, /*isStop*/true);
    s_axis[ax].running   = false;
    s_axis[ax].lastSpeed = 0;
    log_i("stop axis %d (node 0x%02X sub %u)", ax, route->nodeId, route->subAxis);

}

static void startAxis(int ax, int speed)
{
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, (uint8_t)ax);
    if (!route || route->where != UC2::RouteEntry::REMOTE) return;

    if (s_axis[ax].running &&
        std::abs(speed - s_axis[ax].lastSpeed) < kSpeedHysteresis) {
        return; // ignore micro-jitter
    }

    emitMotorRemote(route->nodeId, route->subAxis,
                    /*pos*/0, /*speed*/speed,
                    /*accel*/(uint32_t)MAX_ACCELERATION_A,
                    /*isAbs*/false, /*isForever*/true, /*isStop*/false);
    s_axis[ax].running   = true;
    s_axis[ax].lastSpeed = speed;
}

// dir ∈ {-1,0,+1}, speed 0..63 (joystick) or fixed (zoom/focus keys).
static void handleAxis(int ax, int dir, uint8_t speed63, float multiplier)
{
    if (dir == 0 || speed63 == 0) {
        if (s_axis[ax].running) stopAxis(ax);
        return;
    }
    const float norm  = (float)speed63 * kOneOver63f;   // 0..1, linear
    const int   speed = (int)(dir * norm * kMaxSpeed * multiplier);
    if (speed == 0) { if (s_axis[ax].running) stopAxis(ax); return; }
    startAxis(ax, speed);
}

// ── Upstream key events (TPDO2) ─────────────────────────────────────────────
// We reuse the GPIO-slave TPDO2 mapping (OD x1A01: 4×u8 from x2300 + 2×u16
// from x2310) with a PTZ-specific payload so no OD change is needed:
//   d[0] = EventType (PtzKeyboard::EV_*)     x2300 sub1
//   d[1] = arg (preset / aux number)         x2300 sub2
//   d[2] = sequence number (wraps)           x2300 sub3
//   d[3] = 0x80 magic "PTZ event" flags      x2300 sub4
//   d[4..5] = framesOk counter (diag)        x2310 sub1
//   d[6..7] = 0                              x2310 sub2
// The master matches on its MASTER_PTZ_NODE_ID and the 0x80 flags marker,
// dedupes on the sequence byte, and emits {"ptz":{...}} on its serial port
// (forwardPtzSlaveTpdo, CANopenModule.cpp).
static uint8_t s_eventSeq = 0;

static void emitEventTpdo(const PtzKeyboard::Event& ev, uint32_t framesOk)
{
#ifdef CAN_CONTROLLER_CANOPEN
    if (CO == nullptr || CO->TPDO == nullptr) return;
    s_eventSeq++;
    OD_RAM.x2300_digital_input_state[0] = ev.type;
    OD_RAM.x2300_digital_input_state[1] = ev.arg;
    OD_RAM.x2300_digital_input_state[2] = s_eventSeq;
    OD_RAM.x2300_digital_input_state[3] = 0x80;
    OD_RAM.x2310_analog_input_value[0]  = (uint16_t)framesOk;
    OD_RAM.x2310_analog_input_value[1]  = 0;
    CO_TPDOsendRequest(&CO->TPDO[1]); // TPDO2 = index 1
    log_i("event TPDO2  type=%u arg=%u seq=%u framesOk=%u",
          ev.type, ev.arg, s_eventSeq, (unsigned)framesOk);
#else
    (void)ev; (void)framesOk;
#endif
}

static void stopAllAxes()
{
    stopAxis(Stepper::X);
    stopAxis(Stepper::Y);
    stopAxis(Stepper::Z);
    stopAxis(Stepper::A);
}

static void router_task(void* /*arg*/)
{
    QueueHandle_t evq = PtzKeyboard::getEventQueue();
    const TickType_t period = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t last = xTaskGetTickCount();
    bool anyRunning = false;

    for (;;) {
        vTaskDelayUntil(&last, period);

        // Discrete key events → upstream TPDO2 (never handled locally).
        // Emit at most ONE per tick: emitEventTpdo stages the payload into
        // the shared OD_RAM.x2300 slot and only *requests* an async TPDO2
        // send. Draining several in one tick would let a later event
        // overwrite x2300 before the CANopen stack has transmitted the
        // earlier frame — the master would silently miss events. One event
        // per 20 ms tick is far faster than any keypress, and the 16-deep
        // queue absorbs bursts.
        PtzKeyboard::Event ev;
        if (evq && xQueueReceive(evq, &ev, 0) == pdTRUE)
            emitEventTpdo(ev, 0);

        PtzKeyboard::Motion m;
        if (!PtzKeyboard::getMotion(m)) continue; // nothing received yet

        // Motion watchdog: the keyboard sends frames on change (plus a stop
        // frame on release). If a stop frame is lost and the keyboard goes
        // silent while an axis runs, halt everything after the timeout.
        const uint32_t timeout = PtzKeyboard::motionTimeoutMs();
        if (timeout > 0 && anyRunning &&
            (uint32_t)(millis() - m.updatedMs) > timeout) {
                log_i("motion watchdog: no PTZ frame for %u ms — stopping all axes", (unsigned)timeout);
            stopAllAxes();
            anyRunning = false;
            continue;
        }

        handleAxis(Stepper::X, m.panDir,  m.panSpeed,  pinConfig.PTZ_SPEED_MULTIPLIER_XY);
        handleAxis(Stepper::Y, m.tiltDir, m.tiltSpeed, pinConfig.PTZ_SPEED_MULTIPLIER_XY);
        // Zoom / focus keys carry no speed — use a fixed mid-scale drive.
        handleAxis(Stepper::Z, m.zoomDir,  32, pinConfig.PTZ_SPEED_MULTIPLIER_Z);
        handleAxis(Stepper::A, m.focusDir, 32, pinConfig.PTZ_SPEED_MULTIPLIER_A);

        anyRunning = s_axis[Stepper::X].running || s_axis[Stepper::Y].running ||
                     s_axis[Stepper::Z].running || s_axis[Stepper::A].running;
    }
}

} // namespace

namespace PtzRouter {

bool begin()
{
    static bool started = false;
    if (started) return true;
    BaseType_t ok = xTaskCreatePinnedToCore(router_task, "ptz_router",
                                            4096, NULL, 3, NULL, 0);
    if (ok != pdTRUE) {
        log_e( "router task create failed");
        return false;
    }
    started = true;
    log_i("PTZ → CAN router task created");
    return true;
}

} // namespace PtzRouter

#endif // PTZ_KEYBOARD_CONTROLLER
