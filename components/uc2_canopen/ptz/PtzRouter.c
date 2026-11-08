// PtzRouter.c — PTZ-keyboard → CANopen SDO dispatcher.
// Mirrors JoystickRouter.cpp (the DS4 USB bridge): a 50 Hz task polls
// PtzKeyboard's Motion, issues the 6-frame REMOTE motor bursts against
// the routing table, and forwards discrete key events upstream via TPDO2.
// T4.4 4i: C-ified (was PtzRouter.cpp, namespace PtzRouter).
#include "PinConfig.h"
#include "PtzRouter.h"

#ifdef PTZ_KEYBOARD_CONTROLLER

#include "uc2_hal.h"
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "PtzKeyboard.h"
#include "CANopenModule.h"
#include "RoutingTable.h"
#include "UC2_OD_Indices.h"
#include "MotorTypes.h"

#ifdef CAN_CONTROLLER_CANOPEN
#include <CANopen.h>
#include "OD.h"
#endif

static const char* TAG = "ptz_router";

// Same top speed as the DS4 path (JoystickRouter kMaxSpeed) so the two
// input devices feel identical; per-axis multipliers come from PinConfig.
#define K_MAX_SPEED       2000.0f
#define K_ONE_OVER_63F    (1.0f / 63.0f)
#define K_SPEED_HYSTERESIS 50

#define K_SDO_TIMEOUT_MS       40
#define K_DEAD_NODE_FAIL_THRESHOLD 2
#define K_DEAD_NODE_BACKOFF_MS   2000

typedef struct {
    uint8_t  nodeId;
    uint8_t  fails;
    uint32_t silentUntilMs;
} node_health_t;

static node_health_t s_health[8] = {0};

static node_health_t* health_slot(uint8_t nodeId)
{
    for (int i = 0; i < 8; ++i)
        if (s_health[i].nodeId == nodeId) return &s_health[i];
    for (int i = 0; i < 8; ++i)
        if (s_health[i].nodeId == 0) { s_health[i].nodeId = nodeId; return &s_health[i]; }
    return &s_health[0];
}

static inline bool node_muted(uint8_t nodeId)
{
    return millis() < health_slot(nodeId)->silentUntilMs;
}

static inline void note_sdo(uint8_t nodeId, bool ok)
{
    node_health_t* h = health_slot(nodeId);
    if (ok) { h->fails = 0; h->silentUntilMs = 0; return; }
    if (++h->fails >= K_DEAD_NODE_FAIL_THRESHOLD) {
        h->silentUntilMs = millis() + K_DEAD_NODE_BACKOFF_MS;
        log_w("node 0x%02X muted for %u ms (SDO timeouts)",
              nodeId, (unsigned)K_DEAD_NODE_BACKOFF_MS);
    }
}

typedef struct {
    bool    running;
    int32_t lastSpeed;
} axis_runtime_t;

static axis_runtime_t s_axis[4] = {0}; // indexed by Stepper enum (A=0, X=1, Y=2, Z=3)

// Issue the 6-frame REMOTE motor command. Mirrors JoystickRouter.cpp /
// serial_dispatch.c motor_act (2026-09-30).
static void emit_motor_remote(uint8_t nodeId, uint8_t subAxis,
                              int32_t pos, int32_t speed, uint32_t accel,
                              bool isAbs, bool isForever, bool isStop)
{
    if (node_muted(nodeId)) return;
    const uint8_t sub = subAxis + 1;
    bool ok = true;
    ok &= canopen_sdo_write_i32(nodeId, UC2_OD_MOTOR_TARGET_POSITION, sub, pos, K_SDO_TIMEOUT_MS);
    ok &= canopen_sdo_write_u32(nodeId, UC2_OD_MOTOR_SPEED, sub, (uint32_t)speed, K_SDO_TIMEOUT_MS);
    if (accel > 0)
        ok &= canopen_sdo_write_u32(nodeId, UC2_OD_MOTOR_ACCELERATION, sub, accel, K_SDO_TIMEOUT_MS);
    ok &= canopen_sdo_write_u8(nodeId, UC2_OD_MOTOR_IS_ABSOLUTE, sub, isAbs ? 1 : 0, K_SDO_TIMEOUT_MS);
    ok &= canopen_sdo_write_u8(nodeId, UC2_OD_MOTOR_IS_FOREVER, sub, isForever ? 1 : 0, K_SDO_TIMEOUT_MS);
    const uint8_t cmdWord = isStop
        ? (uint8_t)(1u << (subAxis + 4))
        : (uint8_t)(1u << subAxis);
    ok &= canopen_sdo_write_u8(nodeId, UC2_OD_MOTOR_COMMAND_WORD, 0x00, cmdWord, K_SDO_TIMEOUT_MS);
    note_sdo(nodeId, ok);
}

static void stop_axis(int ax)
{
    const route_entry* route = routing_table_find(ROUTE_ENTRY_TYPE_MOTOR, (uint8_t)ax);
    if (!route || route->where != ROUTE_ENTRY_WHERE_REMOTE) {
        s_axis[ax].running = false;
        return;
    }
    emit_motor_remote(route->nodeId, route->subAxis,
                      /*pos*/0, /*speed*/0, /*accel*/0,
                      /*isAbs*/false, /*isForever*/false, /*isStop*/true);
    s_axis[ax].running   = false;
    s_axis[ax].lastSpeed = 0;
    log_i("stop axis %d (node 0x%02X sub %u)", ax, route->nodeId, route->subAxis);
}

static void start_axis(int ax, int speed)
{
    const route_entry* route = routing_table_find(ROUTE_ENTRY_TYPE_MOTOR, (uint8_t)ax);
    if (!route || route->where != ROUTE_ENTRY_WHERE_REMOTE) return;

    if (s_axis[ax].running &&
        (int)(speed - s_axis[ax].lastSpeed) < K_SPEED_HYSTERESIS) {
        return; // ignore micro-jitter
    }

    emit_motor_remote(route->nodeId, route->subAxis,
                      /*pos*/0, /*speed*/speed,
                      /*accel*/(uint32_t)MAX_ACCELERATION_A,
                      /*isAbs*/false, /*isForever*/true, /*isStop*/false);
    s_axis[ax].running   = true;
    s_axis[ax].lastSpeed = speed;
}

// dir ∈ {-1,0,+1}, speed 0..63 (joystick) or fixed (zoom/focus keys).
static void handle_axis(int ax, int dir, uint8_t speed63, float multiplier)
{
    if (dir == 0 || speed63 == 0) {
        if (s_axis[ax].running) stop_axis(ax);
        return;
    }
    const float norm  = (float)speed63 * K_ONE_OVER_63F;   // 0..1, linear
    const int   speed = (int)(dir * norm * K_MAX_SPEED * multiplier);
    if (speed == 0) { if (s_axis[ax].running) stop_axis(ax); return; }
    start_axis(ax, speed);
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

static void emit_event_tpdo(const ptz_event_t* ev, uint32_t framesOk)
{
#ifdef CAN_CONTROLLER_CANOPEN
    if (CO == NULL || CO->TPDO == NULL) return;
    s_eventSeq++;
    OD_RAM.x2300_digital_input_state[0] = ev->type;
    OD_RAM.x2300_digital_input_state[1] = ev->arg;
    OD_RAM.x2300_digital_input_state[2] = s_eventSeq;
    OD_RAM.x2300_digital_input_state[3] = 0x80;
    OD_RAM.x2310_analog_input_value[0]  = (uint16_t)framesOk;
    OD_RAM.x2310_analog_input_value[1]  = 0;
    CO_TPDOsendRequest(&CO->TPDO[1]); // TPDO2 = index 1
    log_i("event TPDO2  type=%u arg=%u seq=%u framesOk=%u",
          ev->type, ev->arg, s_eventSeq, (unsigned)framesOk);
#else
    (void)ev; (void)framesOk;
#endif
}

static void stop_all_axes(void)
{
    stop_axis(0); // A
    stop_axis(1); // X
    stop_axis(2); // Y
    stop_axis(3); // Z
}

static void router_task(void* arg)
{
    (void)arg;
    QueueHandle_t evq = ptz_keyboard_get_event_queue();
    const TickType_t period = pdMS_TO_TICKS(20); // 50 Hz
    TickType_t last = xTaskGetTickCount();
    bool anyRunning = false;

    for (;;) {
        vTaskDelayUntil(&last, period);

        // Discrete key events → upstream TPDO2 (never handled locally).
        // Emit at most ONE per tick: emit_event_tpdo stages the payload into
        // the shared OD_RAM.x2300 slot and only *requests* an async TPDO2
        // send. Draining several in one tick would let a later event
        // overwrite x2300 before the CANopen stack has transmitted the
        // earlier frame — the master would silently miss events. One event
        // per 20 ms tick is far faster than any keypress, and the 16-deep
        // queue absorbs bursts.
        ptz_event_t ev;
        if (evq && xQueueReceive(evq, &ev, 0) == pdTRUE)
            emit_event_tpdo(&ev, 0);

        ptz_motion_t m;
        if (!ptz_keyboard_get_motion(&m)) continue; // nothing received yet

        // Motion watchdog: the keyboard sends frames on change (plus a stop
        // frame on release). If a stop frame is lost and the keyboard goes
        // silent while an axis runs, halt everything after the timeout.
        const uint32_t timeout = ptz_keyboard_motion_timeout_ms();
        if (timeout > 0 && anyRunning &&
            (uint32_t)(millis() - m.updatedMs) > timeout) {
            log_i("motion watchdog: no PTZ frame for %u ms — stopping all axes", (unsigned)timeout);
            stop_all_axes();
            anyRunning = false;
            continue;
        }

        handle_axis(1, m.panDir,  m.panSpeed,  pinConfig.PTZ_SPEED_MULTIPLIER_XY);
        handle_axis(2, m.tiltDir, m.tiltSpeed, pinConfig.PTZ_SPEED_MULTIPLIER_XY);
        // Zoom / focus keys carry no speed — use a fixed mid-scale drive.
        handle_axis(3, m.zoomDir,  32, pinConfig.PTZ_SPEED_MULTIPLIER_Z);
        handle_axis(0, m.focusDir, 32, pinConfig.PTZ_SPEED_MULTIPLIER_A);

        anyRunning = s_axis[1].running || s_axis[2].running ||
                     s_axis[3].running || s_axis[0].running;
    }
}

bool ptz_router_begin(void)
{
    static bool started = false;
    if (started) return true;
    BaseType_t ok = xTaskCreatePinnedToCore(router_task, "ptz_router",
                                            4096, NULL, 3, NULL, 0);
    if (ok != pdTRUE) {
        log_e("PTZ router task create failed");
        return false;
    }
    started = true;
    log_i("PTZ → CAN router task created");
    return true;
}

#endif // PTZ_KEYBOARD_CONTROLLER