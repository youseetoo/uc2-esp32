// JoystickRouter.c — Bridge-side DS4 → CANopen SDO dispatcher (pure C).
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

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "uc2_hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "JoystickUsbHost.h"
#include "PinConfig.h"
#include "CANopenModule.h"
#include "RoutingTable.h"
#include "JsonKeys.h"
#include "UC2_OD_Indices.h"
#include "MotorTypes.h"

static const char *TAG = "ds4_router";

// Same dead-zone / curve constants as MotorGamePad.cpp so the feel is
// identical to the master's BT path.
static const int kOffset = 1025;
static const float kAlpha = 0.40f;
static const float kMaxSpeed = 5000.0f;
static const float kOneOver32768f = 1.0f / 32768.0f;

// Speed delta below which we don't re-issue a motor SDO burst. Keeps the
// bus quiet while the operator hovers the stick.
static const int kSpeedHysteresis = 50;

// "Send and forget": short SDO timeout so an unresponsive slave doesn't
// stall the 100 Hz router task. A healthy expedited SDO completes in 1-3
// CAN frames (<5 ms at 500 kbit/s); 40 ms is generous for retries while
// keeping a 6-frame dead-node burst under one tick period.
static const uint32_t kSdoTimeoutMs = 40;

// After this many consecutive failed SDOs to the same node, suppress
// further writes for kDeadNodeBackoffMs to keep the bus / task free.
static const uint8_t kDeadNodeFailThreshold = 2;
static const uint32_t kDeadNodeBackoffMs = 2000;

typedef struct {
    uint8_t nodeId;
    uint8_t fails;
    uint32_t silentUntilMs;
} NodeHealth;

static NodeHealth s_health[8];

static NodeHealth *health_slot(uint8_t nodeId)
{
    for (int i = 0; i < 8; i++) {
        if (s_health[i].nodeId == nodeId)
            return &s_health[i];
    }
    for (int i = 0; i < 8; i++) {
        if (s_health[i].nodeId == 0) {
            s_health[i].nodeId = nodeId;
            return &s_health[i];
        }
    }
    s_health[0].nodeId = nodeId;
    return &s_health[0];
}

static bool node_muted(uint8_t nodeId)
{
    NodeHealth *h = health_slot(nodeId);
    return (millis() < h->silentUntilMs);
}

static void note_sdo(uint8_t nodeId, bool ok)
{
    NodeHealth *h = health_slot(nodeId);
    if (ok) {
        h->fails = 0;
        h->silentUntilMs = 0;
        return;
    }
    if (++h->fails >= kDeadNodeFailThreshold) {
        h->silentUntilMs = millis() + kDeadNodeBackoffMs;
        ESP_LOGW(TAG, "node 0x%02X muted for %u ms (SDO timeouts)",
                 nodeId, (unsigned)kDeadNodeBackoffMs);
    }
}

typedef struct {
    bool running;
    int32_t lastSpeed;
} AxisRuntime;

// Axis enum: A=0, X=1, Y=2, Z=3 (same as Stepper enum)
static AxisRuntime s_axis[4];

// Per-axis joystick speed-scaling multiplier. Defaults to the compile-time
// pinConfig.JOYSTICK_SPEED_MULTIPLIER[_Z] values, but can be overridden at
// runtime via /joystick_act and is persisted to Preferences (NVS) so the
// override survives a reboot. Indexed by Stepper enum, same as s_axis.
static float s_speedMultiplier[4];

static const char *kPrefsNamespace = "UC2";

static float default_speed_multiplier(int ax)
{
    return (ax == 3 /* Stepper::Z */) ? (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z
                                      : (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER;
}

// Load persisted multipliers (falling back to pinConfig defaults) once at
// startup, mirroring how FocusMotor::fill_data() restores "joyDir" + axis.
static void load_speed_multipliers()
{
    Preferences prefs;
    prefs.begin(kPrefsNamespace, false);
    for (int ax = 0; ax < 4; ax++) {
        char key[16];
        snprintf(key, sizeof(key), "joySpd%d", ax);
        s_speedMultiplier[ax] = prefs.getFloat(key, default_speed_multiplier(ax));
    }
    prefs.end();
}

// Edge-detect previous button state. button1/2/3 packing matches DS4Report_t
// in JoystickUsbHost.cpp.
static uint8_t s_prevButton1 = 0;
static uint8_t s_prevButton2 = 0;
static uint8_t s_prevDpad = 8; // released

// Laser / LED bridge state — owned by this task only.
static bool s_laserOn = false;
static uint16_t s_laserIntensity = 32768; // mid-scale by default
static uint8_t s_ledPattern = 0;

static float curve_value(int16_t raw)
{
    const int16_t centred = raw - (raw >= 0 ? kOffset : -kOffset);
    const float x = (float)centred * kOneOver32768f;
    const float absX = fabsf(x);
    const float signX = (x >= 0.f) ? 1.f : -1.f;
    if (absX <= kAlpha)
        return x;
    const float diff = absX - kAlpha;
    const float scale = 1.f - kAlpha;
    const float quad = (diff * diff) / (scale * scale);
    return signX * (kAlpha + quad * (1.f - kAlpha));
}

// Issue the 6-frame REMOTE motor command. Mirrors DeviceRouter.cpp:285-315.
// Uses short SDO timeouts + dead-node suppression so a missing slave can't
// stall the router task.
static void emit_motor_remote(uint8_t nodeId, uint8_t subAxis,
                              int32_t pos, int32_t speed, uint32_t accel,
                              bool isAbs, bool isForever, bool isStop)
{
    if (node_muted(nodeId))
        return;
    const uint8_t sub = subAxis + 1;
    bool ok = true;
    ok &= CANopenModule_writeSDO_i32(nodeId, UC2_OD_MOTOR_TARGET_POSITION, sub, pos, kSdoTimeoutMs);
    // Preserve sign: slave decodes MOTOR_SPEED as signed (matches DeviceRouter.cpp:293).
    ok &= CANopenModule_writeSDO_u32(nodeId, UC2_OD_MOTOR_SPEED, sub, (uint32_t)speed, kSdoTimeoutMs);
    if (accel > 0)
        ok &= CANopenModule_writeSDO_u32(nodeId, UC2_OD_MOTOR_ACCELERATION, sub, accel, kSdoTimeoutMs);
    ok &= CANopenModule_writeSDO_u8(nodeId, UC2_OD_MOTOR_IS_ABSOLUTE, sub, isAbs ? 1 : 0, kSdoTimeoutMs);
    ok &= CANopenModule_writeSDO_u8(nodeId, UC2_OD_MOTOR_IS_FOREVER, sub, isForever ? 1 : 0, kSdoTimeoutMs);
    const uint8_t cmdWord = isStop
        ? (uint8_t)(1u << (subAxis + 4))
        : (uint8_t)(1u << subAxis);
    ok &= CANopenModule_writeSDO_u8(nodeId, UC2_OD_MOTOR_COMMAND_WORD, 0x00, cmdWord, kSdoTimeoutMs);
    note_sdo(nodeId, ok);
}

static void stop_axis(int ax)
{
    const RouteEntry *route = routing_table_find(RouteEntry_MOTOR, (uint8_t)ax);
    if (!route || route->where != RouteEntry_REMOTE) {
        s_axis[ax].running = false;
        return;
    }
    emit_motor_remote(route->nodeId, route->subAxis,
                      /*pos*/0, /*speed*/0, /*accel*/0,
                      /*isAbs*/false, /*isForever*/false, /*isStop*/true);
    s_axis[ax].running = false;
    s_axis[ax].lastSpeed = 0;
    ESP_LOGI(TAG, "stop axis %d", ax);
}

static void start_axis(int ax, int speed)
{
    const RouteEntry *route = routing_table_find(RouteEntry_MOTOR, (uint8_t)ax);
    if (!route || route->where != RouteEntry_REMOTE)
        return;

    if (s_axis[ax].running &&
        abs(speed - s_axis[ax].lastSpeed) < kSpeedHysteresis) {
        return; // ignore micro-jitter
    }

    emit_motor_remote(route->nodeId, route->subAxis,
                      /*pos*/0, /*speed*/speed,
                      /*accel*/(uint32_t)MAX_ACCELERATION_A,
                      /*isAbs*/false, /*isForever*/true, /*isStop*/false);
    s_axis[ax].running = true;
    s_axis[ax].lastSpeed = speed;
}

static void single_step(int ax, int32_t position)
{
    const RouteEntry *route = routing_table_find(RouteEntry_MOTOR, (uint8_t)ax);
    if (!route || route->where != RouteEntry_REMOTE)
        return;
    emit_motor_remote(route->nodeId, route->subAxis,
                      position, /*speed*/20000,
                      /*accel*/(uint32_t)MAX_ACCELERATION_A,
                      /*isAbs*/false, /*isForever*/false, /*isStop*/false);
}

static void handle_axis(int16_t value, int ax)
{
    if (abs(value) <= kOffset) {
        if (s_axis[ax].running)
            stop_axis(ax);
        return;
    }
    // Z⇄A mutual exclusion (same as MotorGamePad).
    if (ax == 3 /* Stepper::Z */ && s_axis[0].running)
        stop_axis(0); // Stepper::A
    if (ax == 0 /* Stepper::A */ && s_axis[3].running)
        stop_axis(3); // Stepper::Z

    float speed = curve_value(value) * kMaxSpeed;
    speed *= s_speedMultiplier[ax];
    start_axis(ax, (int)speed);
}

// Edge: returns true on this tick if `bit` flipped from 0→1 in `cur` vs `prev`.
static bool rising(uint8_t cur, uint8_t prev, uint8_t mask)
{
    return (cur & mask) && !(prev & mask);
}

static void emit_laser_remote(uint16_t pwmVal)
{
    const RouteEntry *route = routing_table_find(RouteEntry_LASER, 0);
    if (!route || route->where != RouteEntry_REMOTE)
        return;
    if (node_muted(route->nodeId))
        return;
    bool ok = CANopenModule_writeSDO_u16(route->nodeId,
                                         UC2_OD_LASER_PWM_VALUE,
                                         (uint8_t)(route->subAxis + 1), pwmVal,
                                         kSdoTimeoutMs);
    note_sdo(route->nodeId, ok);
    ESP_LOGI(TAG, "laser -> node 0x%02X = %u (%s)", route->nodeId, pwmVal, ok ? "ok" : "fail");
}

static void emit_led_pattern(uint8_t patternId)
{
    const RouteEntry *route = routing_table_find(RouteEntry_LED, 0);
    if (!route || route->where != RouteEntry_REMOTE)
        return;
    if (node_muted(route->nodeId))
        return;
    bool ok = true;
    ok &= CANopenModule_writeSDO_u8(route->nodeId, UC2_OD_LED_ARRAY_MODE, 0, 1, kSdoTimeoutMs);
    ok &= CANopenModule_writeSDO_u8(route->nodeId, UC2_OD_LED_BRIGHTNESS, 0, 128, kSdoTimeoutMs);
    ok &= CANopenModule_writeSDO_u8(route->nodeId, UC2_OD_LED_PATTERN_ID, 0, patternId, kSdoTimeoutMs);
    note_sdo(route->nodeId, ok);
    ESP_LOGI(TAG, "led pattern -> %u (%s)", patternId, ok ? "ok" : "fail");
}

static void handle_buttons(const Ds4State *s)
{
    // button1: bit4=Square, bit5=Cross, bit6=Circle, bit7=Triangle
    if (rising(s->button1, s_prevButton1, 0x80)) {
        s_laserOn = !s_laserOn;
        emit_laser_remote(s_laserOn ? s_laserIntensity : 0);
    }
    if (rising(s->button1, s_prevButton1, 0x10)) {
        s_ledPattern = (s_ledPattern + 1) % 8;
        emit_led_pattern(s_ledPattern);
    }

    // button2: bit0=L1, bit1=R1, bit2=L2, bit3=R2 (button-style; analog L2/R2
    // already exposed as l2/r2). Mirror MotorGamePad::singlestep_event.
    if (rising(s->button2, s_prevButton2, 0x02))
        single_step(3, 1); // R1 -> Stepper::Z
    if (rising(s->button2, s_prevButton2, 0x08))
        single_step(3, 10); // R2 -> Stepper::Z
    if (rising(s->button2, s_prevButton2, 0x01))
        single_step(3, -1); // L1 -> Stepper::Z
    if (rising(s->button2, s_prevButton2, 0x04))
        single_step(3, -10); // L2 -> Stepper::Z

    // D-pad up/down → laser intensity adjust. 0=N, 2=E, 4=S, 6=W (8-way).
    if (s->dpad != s_prevDpad) {
        int delta = 0;
        if (s->dpad == 0 || s->dpad == 1 || s->dpad == 7)
            delta = +4096;
        if (s->dpad == 3 || s->dpad == 4 || s->dpad == 5)
            delta = -4096;
        if (delta) {
            int v = (int)s_laserIntensity + delta;
            if (v < 0)
                v = 0;
            if (v > 65535)
                v = 65535;
            s_laserIntensity = (uint16_t)v;
            if (s_laserOn)
                emit_laser_remote(s_laserIntensity);
        }
    }

    s_prevButton1 = s->button1;
    s_prevButton2 = s->button2;
    s_prevDpad = s->dpad;
}

static void router_task(void *arg)
{
    (void)arg;
    QueueHandle_t q = joystick_usbhost_get_queue();
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

        Ds4State s = {};
        if (xQueuePeek(q, &s, 0) != pdTRUE)
            continue;

        if (!s.valid) {
            if (wasValid) {
                ESP_LOGW(TAG, "DS4 disconnected — stopping all axes");
                stop_axis(1); // Stepper::X
                stop_axis(2); // Stepper::Y
                stop_axis(3); // Stepper::Z
                stop_axis(0); // Stepper::A
                if (s_laserOn) {
                    s_laserOn = false;
                    emit_laser_remote(0);
                }
            }
            wasValid = false;
            continue;
        }
        wasValid = true;

        // Stick mapping mirrors MotorGamePad::xyza_changed_event(x,y,z,a) ←
        // (lx, ly, ry, rx).  Right-stick Y → Z (focus), right-stick X → A.
        handle_axis(s.lx, 1); // Stepper::X
        handle_axis(s.ly, 2); // Stepper::Y
        handle_axis(s.ry, 3); // Stepper::Z
        handle_axis(s.rx, 0); // Stepper::A

        handle_buttons(&s);
    }
}

// {"task":"/joystick_act", "speedmult": {"steppers": [{"stepperid": 1, "multiplier": 75}]}}
// stepperid follows the Stepper enum (A=0, X=1, Y=2, Z=3), same as /motor_act.
cJSON *joystick_router_act(cJSON *doc)
{
    cJSON *speedMultObj = cJSON_GetObjectItemCaseSensitive(doc, "speedmult");
    if (speedMultObj) {
        cJSON *stprs = cJSON_GetObjectItemCaseSensitive(speedMultObj, key_steppers);
        cJSON *stp = NULL;
        cJSON_ArrayForEach(stp, stprs)
        {
            cJSON *idItem = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid);
            cJSON *multItem = cJSON_GetObjectItemCaseSensitive(stp, "multiplier");
            if (!cJSON_IsNumber(idItem) || !cJSON_IsNumber(multItem))
                continue;
            int axis = idItem->valueint;
            if (axis < 0 || axis >= 4)
                continue;
            float multiplier = (float)multItem->valuedouble;

            s_speedMultiplier[axis] = multiplier;

            char key[16];
            snprintf(key, sizeof(key), "joySpd%d", axis);
            Preferences prefs;
            prefs.begin(kPrefsNamespace, false);
            prefs.putFloat(key, multiplier);
            prefs.end();
            ESP_LOGI(TAG, "Set joystick speed multiplier: axis %d = %.2f", axis, multiplier);
        }
    }
    return joystick_router_get(doc);
}

// {"task":"/joystick_get"} → current per-axis speed multipliers
cJSON *joystick_router_get(cJSON *doc)
{
    (void)doc;
    cJSON *ret = cJSON_CreateObject();
    if (!ret)
        return NULL;
    cJSON *speedMultObj = cJSON_AddObjectToObject(ret, "speedmult");
    if (!speedMultObj)
        return ret;
    cJSON *stprs = cJSON_AddArrayToObject(speedMultObj, key_steppers);
    if (!stprs)
        return ret;
    for (int ax = 0; ax < 4; ax++) {
        cJSON *item = cJSON_CreateObject();
        if (!item)
            continue;
        cJSON_AddNumberToObject(item, key_stepperid, ax);
        cJSON_AddNumberToObject(item, "multiplier", s_speedMultiplier[ax]);
        cJSON_AddItemToArray(stprs, item);
    }
    return ret;
}

bool joystick_router_begin(void)
{
    static bool started = false;
    if (started)
        return true;
    load_speed_multipliers();
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

#endif // JOYSTICK_USBHOST_PROVIDER