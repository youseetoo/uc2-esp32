// JoystickRouter.cpp — Bridge-side DS4 → CANopen SDO dispatcher.
//
// Runs on the ESP32-S3 USB-host bridge node (NODE_ROLE=2, no actuator
// controllers compiled in). Reads decoded DS4 state from
// JoystickUsbHost::getQueue() and issues expedited SDO writes that mirror
// DeviceRouter's REMOTE branches:
//   - motor:  DeviceRouter.cpp:285-315  (6 writes per axis change)
//   - laser:  DeviceRouter.cpp:464-498  (1 write)
//   - LED:    DeviceRouter.cpp:615-696  (1..5 writes)
//
// DeviceRouter itself can't be used here: handleMotorAct/LaserAct/LedAct
// are #ifdef MOTOR_CONTROLLER / LASER_CONTROLLER / LED_CONTROLLER, and the
// bridge intentionally has none of those (would drag in FocusMotor, the
// LedController, etc.).
#include "JoystickRouter.h"

#ifdef JOYSTICK_USBHOST_PROVIDER

#include <Arduino.h>
#include <cmath>
#include <cstdlib>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "JoystickUsbHost.h"
#include "PinConfig.h"
#include "../canopen/CANopenModule.h"
#include "../canopen/RoutingTable.h"
#include "../canopen/UC2_OD_Indices.h"
#include "../motor/MotorTypes.h"

static const char* TAG = "ds4_router";

namespace {

// Same dead-zone / curve constants as MotorGamePad.cpp so the feel is
// identical to the master's BT path.
constexpr int   kOffset        = 1025;
constexpr float kAlpha         = 0.40f;
constexpr float kMaxSpeed      = 2000.0f;
constexpr float kOneOver32768f = 1.0f / 32768.0f;

// Speed delta below which we don't re-issue a motor SDO burst. Keeps the
// bus quiet while the operator hovers the stick.
constexpr int kSpeedHysteresis = 50;

// "Send and forget": short SDO timeout so an unresponsive slave doesn't
// stall the 100 Hz router task. A healthy expedited SDO completes in 1–3
// CAN frames (<5 ms at 500 kbit/s); 40 ms is generous for retries while
// keeping a 6-frame dead-node burst under one tick period.
constexpr uint32_t kSdoTimeoutMs = 40;

// After this many consecutive failed SDOs to the same node, suppress
// further writes for kDeadNodeBackoffMs to keep the bus / task free.
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
    NodeHealth* h = healthSlot(nodeId);
    return millis() < h->silentUntilMs;
}

static inline void noteSdo(uint8_t nodeId, bool ok) {
    NodeHealth* h = healthSlot(nodeId);
    if (ok) { h->fails = 0; h->silentUntilMs = 0; return; }
    if (++h->fails >= kDeadNodeFailThreshold) {
        h->silentUntilMs = millis() + kDeadNodeBackoffMs;
        ESP_LOGW(TAG, "node 0x%02X muted for %u ms (SDO timeouts)",
                 nodeId, (unsigned)kDeadNodeBackoffMs);
    }
}

struct AxisRuntime {
    bool    running    = false;
    int32_t lastSpeed  = 0;
};
AxisRuntime s_axis[4]; // indexed by Stepper enum (A=0, X=1, Y=2, Z=3)

// Edge-detect previous button state. button1/2/3 packing matches DS4Report_t
// in JoystickUsbHost.cpp.
uint8_t s_prevButton1 = 0;
uint8_t s_prevButton2 = 0;
uint8_t s_prevDpad    = 8; // released

// Laser / LED bridge state — owned by this task only.
bool     s_laserOn        = false;
uint16_t s_laserIntensity = 32768; // mid-scale by default
uint8_t  s_ledPattern     = 0;

static inline float curveValue(int16_t raw)
{
    const int16_t centred = raw - (raw >= 0 ? kOffset : -kOffset);
    const float   x       = static_cast<float>(centred) * kOneOver32768f;
    const float   absX    = std::fabs(x);
    const float   signX   = (x >= 0.f) ? 1.f : -1.f;
    if (absX <= kAlpha) return x;
    const float diff  = absX - kAlpha;
    const float scale = 1.f - kAlpha;
    const float quad  = (diff * diff) / (scale * scale);
    return signX * (kAlpha + quad * (1.f - kAlpha));
}

// Issue the 6-frame REMOTE motor command. Mirrors DeviceRouter.cpp:285-315.
// Uses short SDO timeouts + dead-node suppression so a missing slave can't
// stall the router task.
static void emitMotorRemote(uint8_t nodeId, uint8_t subAxis,
                            int32_t pos, int32_t speed, uint32_t accel,
                            bool isAbs, bool isForever, bool isStop)
{
    if (nodeMuted(nodeId)) return;
    const uint8_t sub = subAxis + 1;
    bool ok = true;
    ok &= CANopenModule::writeSDO_i32(nodeId, UC2_OD::MOTOR_TARGET_POSITION, sub, pos, kSdoTimeoutMs);
    // Preserve sign: slave decodes MOTOR_SPEED as signed (matches DeviceRouter.cpp:293).
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
    ESP_LOGI(TAG, "stop axis %d", ax);
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

static void singleStep(int ax, int32_t position)
{
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, (uint8_t)ax);
    if (!route || route->where != UC2::RouteEntry::REMOTE) return;
    emitMotorRemote(route->nodeId, route->subAxis,
                    position, /*speed*/20000,
                    /*accel*/(uint32_t)MAX_ACCELERATION_A,
                    /*isAbs*/false, /*isForever*/false, /*isStop*/false);
}

static void handleAxis(int16_t value, int ax)
{
    if (std::abs(value) <= kOffset) {
        if (s_axis[ax].running) stopAxis(ax);
        return;
    }
    // Z⇄A mutual exclusion (same as MotorGamePad).
    if (ax == Stepper::Z && s_axis[Stepper::A].running) stopAxis(Stepper::A);
    if (ax == Stepper::A && s_axis[Stepper::Z].running) stopAxis(Stepper::Z);

    float speed = curveValue(value) * kMaxSpeed;
    speed *= (ax == Stepper::Z)
                ? pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z
                : pinConfig.JOYSTICK_SPEED_MULTIPLIER;
    startAxis(ax, (int)speed);
}

// Edge: returns true on this tick if `bit` flipped from 0→1 in `cur` vs `prev`.
static inline bool rising(uint8_t cur, uint8_t prev, uint8_t mask)
{
    return (cur & mask) && !(prev & mask);
}

static void emitLaserRemote(uint16_t pwmVal)
{
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::LASER, 0);
    if (!route || route->where != UC2::RouteEntry::REMOTE) return;
    if (nodeMuted(route->nodeId)) return;
    bool ok = CANopenModule::writeSDO_u16(route->nodeId,
                                 UC2_OD::LASER_PWM_VALUE,
                                 (uint8_t)(route->subAxis + 1), pwmVal,
                                 kSdoTimeoutMs);
    noteSdo(route->nodeId, ok);
    ESP_LOGI(TAG, "laser -> node 0x%02X = %u (%s)", route->nodeId, pwmVal, ok?"ok":"fail");
}

static void emitLedPattern(uint8_t patternId)
{
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::LED, 0);
    if (!route || route->where != UC2::RouteEntry::REMOTE) return;
    if (nodeMuted(route->nodeId)) return;
    bool ok = true;
    ok &= CANopenModule::writeSDO_u8(route->nodeId, UC2_OD::LED_ARRAY_MODE, 0, 1, kSdoTimeoutMs);
    ok &= CANopenModule::writeSDO_u8(route->nodeId, UC2_OD::LED_BRIGHTNESS, 0, 128, kSdoTimeoutMs);
    ok &= CANopenModule::writeSDO_u8(route->nodeId, UC2_OD::LED_PATTERN_ID, 0, patternId, kSdoTimeoutMs);
    noteSdo(route->nodeId, ok);
    ESP_LOGI(TAG, "led pattern -> %u (%s)", patternId, ok?"ok":"fail");
}

static void handleButtons(const JoystickUsbHost::Ds4State& s)
{
    // button1: bit4=Square, bit5=Cross, bit6=Circle, bit7=Triangle
    if (rising(s.button1, s_prevButton1, 0x80)) {
        s_laserOn = !s_laserOn;
        emitLaserRemote(s_laserOn ? s_laserIntensity : 0);
    }
    if (rising(s.button1, s_prevButton1, 0x10)) {
        s_ledPattern = (s_ledPattern + 1) % 8;
        emitLedPattern(s_ledPattern);
    }

    // button2: bit0=L1, bit1=R1, bit2=L2, bit3=R2 (button-style; analog L2/R2
    // already exposed as l2/r2). Mirror MotorGamePad::singlestep_event.
    if (rising(s.button2, s_prevButton2, 0x02)) singleStep(Stepper::Z,   1); // R1
    if (rising(s.button2, s_prevButton2, 0x08)) singleStep(Stepper::Z,  10); // R2
    if (rising(s.button2, s_prevButton2, 0x01)) singleStep(Stepper::Z,  -1); // L1
    if (rising(s.button2, s_prevButton2, 0x04)) singleStep(Stepper::Z, -10); // L2

    // D-pad up/down → laser intensity adjust. 0=N, 2=E, 4=S, 6=W (8-way).
    if (s.dpad != s_prevDpad) {
        int delta = 0;
        if (s.dpad == 0 || s.dpad == 1 || s.dpad == 7) delta = +4096;
        if (s.dpad == 3 || s.dpad == 4 || s.dpad == 5) delta = -4096;
        if (delta) {
            int v = (int)s_laserIntensity + delta;
            if (v < 0) v = 0; if (v > 65535) v = 65535;
            s_laserIntensity = (uint16_t)v;
            if (s_laserOn) emitLaserRemote(s_laserIntensity);
        }
    }

    s_prevButton1 = s.button1;
    s_prevButton2 = s.button2;
    s_prevDpad    = s.dpad;
}

static void router_task(void* /*arg*/)
{
    QueueHandle_t q = JoystickUsbHost::getQueue();
    if (!q) {
        ESP_LOGE(TAG, "no DS4 queue — task exiting");
        vTaskDelete(NULL);
        return;
    }

    bool wasValid = false;
    const TickType_t period = pdMS_TO_TICKS(10); // 100 Hz
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last, period);

        JoystickUsbHost::Ds4State s = {};
        if (xQueuePeek(q, &s, 0) != pdTRUE) continue;

        if (!s.valid) {
            if (wasValid) {
                ESP_LOGW(TAG, "DS4 disconnected — stopping all axes");
                stopAxis(Stepper::X);
                stopAxis(Stepper::Y);
                stopAxis(Stepper::Z);
                stopAxis(Stepper::A);
                if (s_laserOn) { s_laserOn = false; emitLaserRemote(0); }
            }
            wasValid = false;
            continue;
        }
        wasValid = true;

        // Stick mapping mirrors MotorGamePad::xyza_changed_event(x,y,z,a) ←
        // (lx, ly, ry, rx).  Right-stick Y → Z (focus), right-stick X → A.
        handleAxis(s.lx, Stepper::X);
        handleAxis(s.ly, Stepper::Y);
        handleAxis(s.ry, Stepper::Z);
        handleAxis(s.rx, Stepper::A);

        handleButtons(s);
    }
}

} // namespace

namespace JoystickRouter {

bool begin()
{
    static bool started = false;
    if (started) return true;
    BaseType_t ok = xTaskCreatePinnedToCore(router_task, "ds4_router",
                                            4096, NULL, 3, NULL, 0);
    if (ok != pdTRUE) {
        ESP_LOGE(TAG, "router task create failed");
        return false;
    }
    started = true;
    ESP_LOGI(TAG, "DS4 → CAN router started");
    return true;
}

} // namespace JoystickRouter

#endif // JOYSTICK_USBHOST_PROVIDER
