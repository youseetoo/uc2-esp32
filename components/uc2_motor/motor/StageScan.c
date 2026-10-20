// T4.1: pure C (was the C++ StageScan namespace).
static const char *TAG = "StageScan";
#include "StageScan.h"
#include "StageScanOrder.h"      // columnFor / channelSequence / forEachFrame / kChannel*
#include "uc2_hal/uc2_log.h"     // log_i / log_w / log_e
#include "uc2_hal/uc2_time.h"    // millis / delayMicroseconds
#include "uc2_hal/uc2_gpio.h"    // pinMode / digitalWrite
#include "PinConfigDefault.h"    // pinConfig
#include "FocusMotor.h"          // focusmotor_get_data / focusmotor_set_enable
#include "JsonKeys.h"            // keyQueueID
#include "cJsonTool.h"           // cjson_tool_set_bool / cjson_tool_set_int
#include "DeviceRouter.h"        // device_router_handle_{motor,laser,led}_act (C shims)
#include "uc2_serial_out.h"      // serial_out_safe_send_json_string
#include <math.h>    // sqrtf
#include <stdlib.h>  // free
#include <string.h>  // memcpy
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// -----------------------------------------------------------------------------
// Everything that touches hardware goes through DeviceRouter, so the same code
// runs on a standalone board (local steppers / PWM lasers / NeoPixels) and on a
// CANopen master whose motors, lasers and LEDs live on slave nodes. The camera
// trigger pin is always local (it is wired to the master / HAT).
//
// Timing model per frame:  move (all changed axes together) -> light on ->
// tPre -> trigger pulse (tTrig) -> tPost -> light off.
// -----------------------------------------------------------------------------
// File-scope initializers must be constants in C, so the NSDMI defaults are
// spelled out here (stage_scan_data_init() is for other translation units).
static StageScanningData sd = { .delayTimeStep = 10, .nFrames = 1 };
=======
volatile bool stage_scan_is_running = false;

StageScanningData *stage_scan_get_data(void)
{
    return &sd;
}

void stage_scan_clear_coordinates(void)
{
    if (sd.coordinates != NULL)
    {
        free(sd.coordinates);
        sd.coordinates = NULL;
    }
    sd.coordinateCount = 0;
    sd.useCoordinates = false;
}

void stage_scan_set_coordinates(StagePosition *coords, int count)
{
    stage_scan_clear_coordinates();
    log_i("Setting %d coordinates for stage scanning", count);
    if (coords != NULL && count > 0)
    {
        sd.coordinates = (StagePosition *)malloc((size_t)count * sizeof(StagePosition));
        if (sd.coordinates != NULL)
        {
            memcpy(sd.coordinates, coords, (size_t)count * sizeof(StagePosition));
            sd.coordinateCount = count;
            sd.useCoordinates = true;
        }
    }
}

// -------------------------------------------------------------------------
// Serial notifications: framed (++/--) through the output queue, so they
// never interleave with responses other tasks are writing.
// -------------------------------------------------------------------------
static void sendJson(cJSON *json)
{
    char *s = cJSON_PrintUnformatted(json);
    if (s)
    {
        serial_out_safe_send_json_string(s);
        free(s);
    }
    cJSON_Delete(json);
}

// -------------------------------------------------------------------------
// Camera trigger: a clean pulse on CAMERA_TRIGGER_PIN, then the
// {"cam":1,"frame":n} notification the host uses for software triggering
// and frame counting. (It used to be printed *inside* the pulse, so the
// pulse width was whatever the UART took.) Without a trigger pin only the
// notification is sent.
// -------------------------------------------------------------------------
static void fireTrigger(uint32_t frameIndex, int pulseMs)
{
    const int pin = pinConfig.CAMERA_TRIGGER_PIN;
    if (pin >= 0)
    {
        const bool inv = pinConfig.CAMERA_TRIGGER_INVERTED;
        digitalWrite(pin, inv ? LOW : HIGH);
        if (pulseMs <= 1)
            delayMicroseconds(1000); // 1 ms floor keeps opto-isolated inputs happy
        else
            vTaskDelay(pdMS_TO_TICKS(pulseMs));
        digitalWrite(pin, inv ? HIGH : LOW);
    }
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "cam", 1);
    cJSON_AddNumberToObject(j, "frame", (double)frameIndex);
    sendJson(j);
}

// -------------------------------------------------------------------------
// Lights. LaserController::setLaserVal / LedController::execLedCommand only
// drive LOCAL hardware; on a CAN master the lasers and the LED array are
// slave nodes, which only DeviceRouter knows how to reach.
// -------------------------------------------------------------------------
static void setLaser(int channel, int value)
{
#ifdef LASER_CONTROLLER
    cJSON *doc = cJSON_CreateObject();
    cJSON_AddNumberToObject(doc, "LASERid", channel);
    cJSON_AddNumberToObject(doc, "LASERval", value);
    cJSON *resp = device_router_handle_laser_act(doc);
    if (resp) cJSON_Delete(resp);
    cJSON_Delete(doc);
#else
    (void)channel; (void)value;
    log_w("stagescan: laser %d requested but LASER_CONTROLLER is not built", channel);
#endif
}

static void setLedArray(int intensity)
{
#ifdef LED_CONTROLLER
    cJSON *doc = cJSON_CreateObject();
    cJSON *led = cJSON_AddObjectToObject(doc, "led");
    if (intensity > 0)
    {
        const int v = intensity > 255 ? 255 : intensity;
        cJSON_AddStringToObject(led, "action", "fill");
        cJSON_AddNumberToObject(led, "r", v);
        cJSON_AddNumberToObject(led, "g", v);
        cJSON_AddNumberToObject(led, "b", v);
    }
    else
    {
        cJSON_AddStringToObject(led, "action", "off");
    }
    cJSON *resp = device_router_handle_led_act(doc);
    if (resp) cJSON_Delete(resp);
    cJSON_Delete(doc);
#else
    (void)intensity;
    log_w("stagescan: LED array requested but LED_CONTROLLER is not built");
#endif
}

static void setChannel(const StageScanningData *s, int channel, bool on)
{
    if (channel == kChannelNone) return;
    if (channel == kChannelLed)
        setLedArray(on ? s->ledarrayIntensity : 0);
    else
        setLaser(channel, on ? s->lightsourceIntensities[channel] : 0);
}

static void allLightsOff(const StageScanningData *s)
{
    for (int j = 0; j < kLaserChannels; ++j)
        if (s->lightsourceIntensities[j] > 0) setLaser(j, 0);
    if (s->ledarrayIntensity > 0) setLedArray(0);
}

// -------------------------------------------------------------------------
// Motion. One motor_act with every changed axis, so X/Y/Z move together
// instead of one after the other (the old moveAbs also slept a fixed
// 100 ms after every single-axis dispatch: >= 300 ms dead time per point).
// -------------------------------------------------------------------------
static const Stepper kAxes[3] = {X, Y, Z};

// Trapezoidal profile estimate in ms; only used for timeouts and the
// nonstop trigger schedule.
static uint32_t travelTimeMs(int32_t from, int32_t to, int32_t speed, int32_t accel)
{
    const int32_t distance = (to > from) ? (to - from) : (from - to);
    if (distance == 0 || speed <= 0 || accel <= 0) return 0;
    const float accelTime = (float)speed / (float)accel;                  // s
    const float accelDist = (float)speed * (float)speed / (2.0f * accel); // steps
    float total;
    if ((float)distance < 2.0f * accelDist)
    {
        const float peak = sqrtf((float)distance * (float)accel);
        total = 2.0f * peak / (float)accel;
    }
    else
    {
        total = 2.0f * accelTime + ((float)distance - 2.0f * accelDist) / (float)speed;
    }
    return (uint32_t)(total * 1000.0f);
}

static void dispatchMove(const int32_t *target, const bool *active, int speed, int accel)
{
    cJSON *doc = cJSON_CreateObject();
    cJSON *motor = cJSON_AddObjectToObject(doc, "motor");
    cJSON *steppers = cJSON_AddArrayToObject(motor, "steppers");
    for (int a = 0; a < 3; ++a)
    {
        if (!active[a]) continue;
        MotorData *d = focusmotor_get_data()[kAxes[a]];
        if (!d) continue;
        d->stopped = false; // completion = the running -> stopped edge (waitForAxes)
        d->isStop = 0;
        cJSON *s = cJSON_CreateObject();
        cJSON_AddNumberToObject(s, "stepperid", (int)kAxes[a]);
        cJSON_AddNumberToObject(s, "position", target[a]);
        cJSON_AddNumberToObject(s, "speed", speed);
        cJSON_AddNumberToObject(s, "acceleration", accel);
        cJSON_AddNumberToObject(s, "isabs", 1);
        cJSON_AddNumberToObject(s, "isforever", 0);
        cJSON_AddItemToArray(steppers, s);
    }
    cJSON *resp = device_router_handle_motor_act(doc);
    if (resp) cJSON_Delete(resp);
    cJSON_Delete(doc);
}

static void stopAxes(void)
{
    cJSON *doc = cJSON_CreateObject();
    cJSON *motor = cJSON_AddObjectToObject(doc, "motor");
    cJSON *steppers = cJSON_AddArrayToObject(motor, "steppers");
    for (int a = 0; a < 3; ++a)
    {
        cJSON *s = cJSON_CreateObject();
        cJSON_AddNumberToObject(s, "stepperid", (int)kAxes[a]);
        cJSON_AddNumberToObject(s, "isStop", 1);
        cJSON_AddItemToArray(steppers, s);
    }
    cJSON *resp = device_router_handle_motor_act(doc);
    if (resp) cJSON_Delete(resp);
    cJSON_Delete(doc);
}

// Blocks until every active axis reports stopped AT its target. "Stopped
// and at target" is deliberate: a slave's status word carries a 1 Hz
// heartbeat toggle, so a stale "not running" frame can reach the master
// between our command and the slave's first "running" report; it still
// carries the old position, so it cannot satisfy this check. Local axes
// refresh currentPosition when they stop (FAccelStep), so no polling of
// the driver is needed here.
static bool waitForAxes(const int32_t *target, const bool *active, uint32_t timeoutMs)
{
    const uint32_t t0 = millis();
    for (;;)
    {
        bool done = true;
        for (int a = 0; a < 3 && done; ++a)
        {
            if (!active[a]) continue;
            MotorData *d = focusmotor_get_data()[kAxes[a]];
            if (d && !(d->stopped && d->currentPosition == target[a])) done = false;
        }
        if (done) return true;
        if (sd.stopped) return false;
        if ((uint32_t)(millis() - t0) > timeoutMs)
        {
            log_e("stagescan: move timeout after %u ms (target X:%d Y:%d Z:%d)",
                  (unsigned)timeoutMs, (int)target[0], (int)target[1], (int)target[2]);
            return false;
        }
        vTaskDelay(1);
    }
}

// Move the requested axes together; axes already at their target are
// skipped. Returns false on timeout or abort.
static bool moveTo(const int32_t *target, const bool *active, int speed, int accel)
{
    bool need[3] = {false, false, false};
    bool any = false;
    uint32_t est = 0;
    for (int a = 0; a < 3; ++a)
    {
        if (!active[a]) continue;
        MotorData *d = focusmotor_get_data()[kAxes[a]];
        if (!d || d->currentPosition == target[a]) continue;
        need[a] = true;
        any = true;
        const uint32_t t = travelTimeMs(d->currentPosition, target[a], speed, accel);
        if (t > est) est = t;
    }
    if (!any) return true;
    dispatchMove(target, need, speed, accel);
    return waitForAxes(target, need, est * 2 + 1000);
}

// -------------------------------------------------------------------------
// One frame: light on -> tPre -> trigger -> tPost -> light off
// -------------------------------------------------------------------------
static void exposeFrame(const StageScanningData *s, int channel, uint32_t frame)
{
    setChannel(s, channel, true);
    if (s->delayTimePreTrigger > 0) vTaskDelay(pdMS_TO_TICKS(s->delayTimePreTrigger));
    fireTrigger(frame, s->delayTimeTrigger);
    if (s->delayTimePostTrigger > 0) vTaskDelay(pdMS_TO_TICKS(s->delayTimePostTrigger));
    setChannel(s, channel, false);
}

typedef struct ScanStats
{
    uint32_t frames;
    uint32_t moveTimeouts;
    bool aborted;
} ScanStats;

// -------------------------------------------------------------------------
// Grid, stop-and-go: the frame order is forEachFrame.
// -------------------------------------------------------------------------
typedef struct GridCtx
{
    const StageScanningData *s;
    ScanStats *st;
    int32_t x0, y0, z0;
    int lastCol, lastRow, lastZ;
} GridCtx;

static bool gridVisit(uint16_t col, uint16_t row, uint16_t iz, int channel, void *user)
{
    GridCtx *g = (GridCtx *)user;
    const StageScanningData *s = g->s;
    if (s->stopped) { g->st->aborted = true; return false; }
    if ((int)col != g->lastCol || (int)row != g->lastRow || (int)iz != g->lastZ)
    {
        const int32_t target[3] = {g->x0 + (int32_t)col * s->xStep,
                                   g->y0 + (int32_t)row * s->yStep,
                                   g->z0 + (int32_t)iz * s->zStep};
        const bool active[3] = {(int)col != g->lastCol, (int)row != g->lastRow, (int)iz != g->lastZ};
        if (!moveTo(target, active, s->speed, s->acceleration))
        {
            if (s->stopped) { g->st->aborted = true; return false; }
            ++g->st->moveTimeouts; // keep going: the frame is taken where the stage got to
        }
        g->lastCol = col; g->lastRow = row; g->lastZ = iz;
    }
    exposeFrame(s, channel, g->st->frames++);
    return true;
}

static void gridStopAndGo(const StageScanningData *s, int32_t x0, int32_t y0, int32_t z0, ScanStats *st)
{
    GridCtx g = {.s = s, .st = st, .x0 = x0, .y0 = y0, .z0 = z0,
                 .lastCol = -1, .lastRow = -1, .lastZ = -1};
    forEachFrame((uint16_t)s->nX, (uint16_t)s->nY, (uint16_t)s->nZ, s->zicZac,
                 s->lightsourceIntensities, s->ledarrayIntensity,
                 gridVisit, &g);
}

// -------------------------------------------------------------------------
// Grid, nonstop: each row is swept continuously and the trigger fires when
// the motion profile says a column is passed. No light switching (as
// before) and no Z stack; nZ is ignored here.
// -------------------------------------------------------------------------
static void gridNonstop(const StageScanningData *s, int32_t x0, int32_t y0, int32_t z0, ScanStats *st)
{
    for (uint16_t iy = 0; iy < (uint16_t)s->nY; ++iy)
    {
        if (s->stopped) { st->aborted = true; return; }
        const int32_t xFirst = x0 + (int32_t)columnFor(0, iy, (uint16_t)s->nX, s->zicZac) * s->xStep;
        const int32_t xLast  = x0 + (int32_t)columnFor((uint16_t)s->nX - 1, iy, (uint16_t)s->nX, s->zicZac) * s->xStep;
        const int32_t rowStart[3] = {xFirst, y0 + (int32_t)iy * s->yStep, z0};
        const bool all[3] = {true, true, true};
        if (!moveTo(rowStart, all, s->speed, s->acceleration))
        {
            if (s->stopped) { st->aborted = true; return; }
            ++st->moveTimeouts;
        }

        const int32_t sweep[3] = {xLast, 0, 0};
        const bool onlyX[3] = {s->nX > 1, false, false};
        const uint32_t tLine = millis();
        if (onlyX[0]) dispatchMove(sweep, onlyX, s->speed, s->acceleration);
        log_i("stagescan nonstop row %u: X %d -> %d", (unsigned)iy, (int)xFirst, (int)xLast);

        for (uint16_t ix = 0; ix < (uint16_t)s->nX; ++ix)
        {
            if (s->stopped) { stopAxes(); st->aborted = true; return; }
            const int32_t xTarget = x0 + (int32_t)columnFor(ix, iy, (uint16_t)s->nX, s->zicZac) * s->xStep;
            const uint32_t due = travelTimeMs(xFirst, xTarget, s->speed, s->acceleration);
            const uint32_t elapsed = millis() - tLine;
            if (due > elapsed) vTaskDelay(pdMS_TO_TICKS(due - elapsed));
            if (s->delayTimePreTrigger > 0) vTaskDelay(pdMS_TO_TICKS(s->delayTimePreTrigger));
            fireTrigger(st->frames++, s->delayTimeTrigger);
            if (s->delayTimePostTrigger > 0) vTaskDelay(pdMS_TO_TICKS(s->delayTimePostTrigger));
        }
        if (onlyX[0] && !waitForAxes(sweep, onlyX, travelTimeMs(xFirst, xLast, s->speed, s->acceleration) * 2 + 1000))
        {
            if (s->stopped) { st->aborted = true; return; }
            ++st->moveTimeouts;
        }
    }
}

// -------------------------------------------------------------------------
// Coordinate list, stop-and-go: move, then one frame per light channel.
// -------------------------------------------------------------------------
static void coordinatesStopAndGo(const StageScanningData *s, ScanStats *st)
{
    int seq[kLaserChannels + 1];
    const int nCh = channelSequence(s->lightsourceIntensities, s->ledarrayIntensity, seq);
    for (int i = 0; i < s->coordinateCount; ++i)
    {
        if (s->stopped) { st->aborted = true; return; }
        const StagePosition *p = &s->coordinates[i];
        const bool hasZ = (p->z != STAGE_SCAN_KEEP_AXIS);
        const int32_t target[3] = {p->x, p->y, hasZ ? p->z : 0};
        const bool active[3] = {true, true, hasZ};
        if (!moveTo(target, active, s->speed, s->acceleration))
        {
            if (s->stopped) { st->aborted = true; return; }
            ++st->moveTimeouts;
        }
        for (int c = 0; c < nCh; ++c)
        {
            if (s->stopped) { st->aborted = true; return; }
            exposeFrame(s, seq[c], st->frames++);
        }
    }
}

// -------------------------------------------------------------------------
// Coordinate list, nonstop: head for the last coordinate and trigger when
// the motion profile says each intermediate one is passed (as before).
// -------------------------------------------------------------------------
static void coordinatesNonstop(const StageScanningData *s, ScanStats *st)
{
    MotorData *dx = focusmotor_get_data()[X];
    MotorData *dy = focusmotor_get_data()[Y];
    const int32_t sx = dx ? dx->currentPosition : 0;
    const int32_t sy = dy ? dy->currentPosition : 0;
    const StagePosition *last = &s->coordinates[s->coordinateCount - 1];
    const int32_t target[3] = {last->x, last->y, 0};
    const bool xy[3] = {true, true, false};
    const uint32_t t0 = millis();
    dispatchMove(target, xy, s->speed, s->acceleration);
    for (int i = 0; i < s->coordinateCount; ++i)
    {
        if (s->stopped) { stopAxes(); st->aborted = true; return; }
        const uint32_t tx = travelTimeMs(sx, s->coordinates[i].x, s->speed, s->acceleration);
        const uint32_t ty = travelTimeMs(sy, s->coordinates[i].y, s->speed, s->acceleration);
        const uint32_t due = tx > ty ? tx : ty;
        const uint32_t elapsed = millis() - t0;
        if (due > elapsed) vTaskDelay(pdMS_TO_TICKS(due - elapsed));
        if (s->delayTimePreTrigger > 0) vTaskDelay(pdMS_TO_TICKS(s->delayTimePreTrigger));
        fireTrigger(st->frames++, s->delayTimeTrigger);
        if (s->delayTimePostTrigger > 0) vTaskDelay(pdMS_TO_TICKS(s->delayTimePostTrigger));
    }
    const uint32_t tx = travelTimeMs(sx, last->x, s->speed, s->acceleration);
    const uint32_t ty = travelTimeMs(sy, last->y, s->speed, s->acceleration);
    if (!waitForAxes(target, xy, (tx > ty ? tx : ty) * 2 + 1000) && !s->stopped) ++st->moveTimeouts;
}

// -------------------------------------------------------------------------
// Entry point (runs in its own task, see MotorJsonParser::parseStageScan)
// -------------------------------------------------------------------------
void stage_scan(bool isThread)
{
    if (stage_scan_is_running)
    {
        log_w("stagescan already running");
        if (isThread) vTaskDelete(NULL);
        return;
    }
    stage_scan_is_running = true;
    ScanStats st = {0};

    focusmotor_set_enable(true);
    if (pinConfig.CAMERA_TRIGGER_PIN >= 0)
    {
        pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
        digitalWrite(pinConfig.CAMERA_TRIGGER_PIN, pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW);
    }
    else
    {
        log_w("stagescan: no CAMERA_TRIGGER_PIN, only {\"cam\":1} notifications will be sent");
    }
    allLightsOff(&sd);

    if (sd.useCoordinates && sd.coordinates != NULL && sd.coordinateCount > 0)
    {
        log_i("stagescan: %d coordinates, nonstop=%d", sd.coordinateCount, (int)sd.nonstop);
        if (sd.nonstop) coordinatesNonstop(&sd, &st);
        else coordinatesStopAndGo(&sd, &st);
    }
    else
    {
        // A start of 0 means "from where the stage is" (what ImSwitch sends).
        MotorData *dx = focusmotor_get_data()[X];
        MotorData *dy = focusmotor_get_data()[Y];
        MotorData *dz = focusmotor_get_data()[Z];
        const int32_t x0 = sd.xStart != 0 ? sd.xStart : (dx ? dx->currentPosition : 0);
        const int32_t y0 = sd.yStart != 0 ? sd.yStart : (dy ? dy->currentPosition : 0);
        const int32_t z0 = sd.zStart != 0 ? sd.zStart : (dz ? dz->currentPosition : 0);
        log_i("stagescan: grid %ux%ux%u from X:%d Y:%d Z:%d step X:%d Y:%d Z:%d nonstop=%d, %u frames",
              (unsigned)sd.nX, (unsigned)sd.nY, (unsigned)sd.nZ, (int)x0, (int)y0, (int)z0,
              (int)sd.xStep, (int)sd.yStep, (int)sd.zStep, (int)sd.nonstop,
              (unsigned)frameCount((uint16_t)sd.nX, (uint16_t)sd.nY, (uint16_t)sd.nZ,
                                   sd.lightsourceIntensities, sd.ledarrayIntensity));
        if (sd.nonstop) gridNonstop(&sd, x0, y0, z0, &st);
        else gridStopAndGo(&sd, x0, y0, z0, &st);
    }

    allLightsOff(&sd); // also covers an abort mid-exposure

    // Completion: {"stagescan":true,"frames":N,"moveTimeouts":k,"aborted":0|1,"qid":q,"success":0|1}
    // The host matches on "stagescan" + "success"; the counters let it
    // tell "all frames fired" from "stopped early" without guessing.
    cJSON *json = cJSON_CreateObject();
    cjson_tool_set_bool(json, "stagescan", true);
    cjson_tool_set_int(json, "frames", (int)st.frames);
    cjson_tool_set_int(json, "moveTimeouts", (int)st.moveTimeouts);
    cjson_tool_set_int(json, "aborted", st.aborted ? 1 : 0);
    cjson_tool_set_int(json, keyQueueID, sd.qid);
    cjson_tool_set_int(json, "success", (st.aborted || st.moveTimeouts) ? 0 : 1);
    sendJson(json);

    stage_scan_is_running = false;
    if (isThread)
        vTaskDelete(NULL);
}

void stage_scan_thread(void *arg)
// Grid:        {"task":"/motor_act","stagescan":{"xStart":0,"yStart":0,"zStart":0,"xStep":500,"yStep":500,"zStep":0,
//               "nX":2,"nY":2,"nZ":1,"tPre":50,"tPost":50,"tTrig":1,"illumination":[0,100,0,0,0],"led":0}}
// Coordinates: {"task":"/motor_act","stagescan":{"coordinates":[{"x":100,"y":200},{"x":300,"y":400,"z":10}],"tPre":50,"tPost":50}}
{
    (void)arg;
    stage_scan(true);
}
