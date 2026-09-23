#include "StageScan.h"
#include "StageScanOrder.h"
#include "Arduino.h"
#include "FocusMotor.h"
#include "../../JsonKeys.h"
#include "../../cJsonTool.h"
#include "../canopen/DeviceRouter.h"
#include "../serial/SerialProcess.h"

// -----------------------------------------------------------------------------
// Everything that touches hardware goes through DeviceRouter, so the same code
// runs on a standalone board (local steppers / PWM lasers / NeoPixels) and on a
// CANopen master whose motors, lasers and LEDs live on slave nodes. The camera
// trigger pin is always local (it is wired to the master / HAT).
//
// Timing model per frame:  move (all changed axes together) -> light on ->
// tPre -> trigger pulse (tTrig) -> tPost -> light off.
// -----------------------------------------------------------------------------
namespace StageScan
{
    StageScanningData stageScanningData;
    volatile bool isRunning = false;

    StageScanningData *getStageScanData()
    {
        return &stageScanningData;
    }

    void setCoordinates(StagePosition *coords, int count)
    {
        clearCoordinates();
        log_i("Setting %d coordinates for stage scanning", count);
        if (coords != nullptr && count > 0)
        {
            stageScanningData.coordinates = new StagePosition[count];
            for (int i = 0; i < count; i++)
                stageScanningData.coordinates[i] = coords[i];
            stageScanningData.coordinateCount = count;
            stageScanningData.useCoordinates = true;
        }
    }

    void clearCoordinates()
    {
        if (stageScanningData.coordinates != nullptr)
        {
            delete[] stageScanningData.coordinates;
            stageScanningData.coordinates = nullptr;
        }
        stageScanningData.coordinateCount = 0;
        stageScanningData.useCoordinates = false;
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
            SerialProcess::safeSendJsonString(s);
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
                ets_delay_us(1000); // 1 ms floor keeps opto-isolated inputs happy
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
        cJSON *resp = DeviceRouter::handleLaserAct(doc);
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
        cJSON *resp = DeviceRouter::handleLedAct(doc);
        if (resp) cJSON_Delete(resp);
        cJSON_Delete(doc);
#else
        (void)intensity;
        log_w("stagescan: LED array requested but LED_CONTROLLER is not built");
#endif
    }

    static void setChannel(const StageScanningData &sd, int channel, bool on)
    {
        if (channel == StageScanOrder::kChannelNone) return;
        if (channel == StageScanOrder::kChannelLed)
            setLedArray(on ? sd.ledarrayIntensity : 0);
        else
            setLaser(channel, on ? sd.lightsourceIntensities[channel] : 0);
    }

    static void allLightsOff(const StageScanningData &sd)
    {
        for (int j = 0; j < StageScanOrder::kLaserChannels; ++j)
            if (sd.lightsourceIntensities[j] > 0) setLaser(j, 0);
        if (sd.ledarrayIntensity > 0) setLedArray(0);
    }

    // -------------------------------------------------------------------------
    // Motion. One motor_act with every changed axis, so X/Y/Z move together
    // instead of one after the other (the old moveAbs also slept a fixed
    // 100 ms after every single-axis dispatch: >= 300 ms dead time per point).
    // -------------------------------------------------------------------------
    static const Stepper kAxes[3] = {Stepper::X, Stepper::Y, Stepper::Z};

    // Trapezoidal profile estimate in ms; only used for timeouts and the
    // nonstop trigger schedule.
    static uint32_t travelTimeMs(int32_t from, int32_t to, int32_t speed, int32_t accel)
    {
        const int32_t distance = (to > from) ? (to - from) : (from - to);
        if (distance == 0 || speed <= 0 || accel <= 0) return 0;
        const float accelTime = (float)speed / (float)accel;                 // s
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
            MotorData *d = FocusMotor::getData()[kAxes[a]];
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
        cJSON *resp = DeviceRouter::handleMotorAct(doc);
        if (resp) cJSON_Delete(resp);
        cJSON_Delete(doc);
    }

    static void stopAxes()
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
        cJSON *resp = DeviceRouter::handleMotorAct(doc);
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
                MotorData *d = FocusMotor::getData()[kAxes[a]];
                if (d && !(d->stopped && d->currentPosition == target[a])) done = false;
            }
            if (done) return true;
            if (stageScanningData.stopped) return false;
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
            MotorData *d = FocusMotor::getData()[kAxes[a]];
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
    static void exposeFrame(const StageScanningData &sd, int channel, uint32_t frame)
    {
        setChannel(sd, channel, true);
        if (sd.delayTimePreTrigger > 0) vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
        fireTrigger(frame, sd.delayTimeTrigger);
        if (sd.delayTimePostTrigger > 0) vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
        setChannel(sd, channel, false);
    }

    struct ScanStats
    {
        uint32_t frames = 0;
        uint32_t moveTimeouts = 0;
        bool aborted = false;
    };

    // -------------------------------------------------------------------------
    // Grid, stop-and-go: the frame order is StageScanOrder::forEachFrame.
    // -------------------------------------------------------------------------
    static void gridStopAndGo(StageScanningData &sd, int32_t x0, int32_t y0, int32_t z0, ScanStats &st)
    {
        int lastCol = -1, lastRow = -1, lastZ = -1;
        StageScanOrder::forEachFrame(
            sd.nX, sd.nY, sd.nZ, sd.zicZac, sd.lightsourceIntensities, sd.ledarrayIntensity,
            [&](uint16_t col, uint16_t row, uint16_t iz, int channel) -> bool
            {
                if (sd.stopped) { st.aborted = true; return false; }
                if ((int)col != lastCol || (int)row != lastRow || (int)iz != lastZ)
                {
                    const int32_t target[3] = {x0 + (int32_t)col * sd.xStep,
                                               y0 + (int32_t)row * sd.yStep,
                                               z0 + (int32_t)iz * sd.zStep};
                    const bool active[3] = {(int)col != lastCol, (int)row != lastRow, (int)iz != lastZ};
                    if (!moveTo(target, active, sd.speed, sd.acceleration))
                    {
                        if (sd.stopped) { st.aborted = true; return false; }
                        ++st.moveTimeouts; // keep going: the frame is taken where the stage got to
                    }
                    lastCol = col; lastRow = row; lastZ = iz;
                }
                exposeFrame(sd, channel, st.frames++);
                return true;
            });
    }

    // -------------------------------------------------------------------------
    // Grid, nonstop: each row is swept continuously and the trigger fires when
    // the motion profile says a column is passed. No light switching (as
    // before) and no Z stack; nZ is ignored here.
    // -------------------------------------------------------------------------
    static void gridNonstop(StageScanningData &sd, int32_t x0, int32_t y0, int32_t z0, ScanStats &st)
    {
        for (uint16_t iy = 0; iy < sd.nY; ++iy)
        {
            if (sd.stopped) { st.aborted = true; return; }
            const int32_t xFirst = x0 + (int32_t)StageScanOrder::columnFor(0, iy, sd.nX, sd.zicZac) * sd.xStep;
            const int32_t xLast  = x0 + (int32_t)StageScanOrder::columnFor(sd.nX - 1, iy, sd.nX, sd.zicZac) * sd.xStep;
            const int32_t rowStart[3] = {xFirst, y0 + (int32_t)iy * sd.yStep, z0};
            const bool all[3] = {true, true, true};
            if (!moveTo(rowStart, all, sd.speed, sd.acceleration))
            {
                if (sd.stopped) { st.aborted = true; return; }
                ++st.moveTimeouts;
            }

            const int32_t sweep[3] = {xLast, 0, 0};
            const bool onlyX[3] = {sd.nX > 1, false, false};
            const uint32_t tLine = millis();
            if (onlyX[0]) dispatchMove(sweep, onlyX, sd.speed, sd.acceleration);
            log_i("stagescan nonstop row %u: X %d -> %d", (unsigned)iy, (int)xFirst, (int)xLast);

            for (uint16_t ix = 0; ix < sd.nX; ++ix)
            {
                if (sd.stopped) { stopAxes(); st.aborted = true; return; }
                const int32_t xTarget = x0 + (int32_t)StageScanOrder::columnFor(ix, iy, sd.nX, sd.zicZac) * sd.xStep;
                const uint32_t due = travelTimeMs(xFirst, xTarget, sd.speed, sd.acceleration);
                const uint32_t elapsed = millis() - tLine;
                if (due > elapsed) vTaskDelay(pdMS_TO_TICKS(due - elapsed));
                if (sd.delayTimePreTrigger > 0) vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                fireTrigger(st.frames++, sd.delayTimeTrigger);
                if (sd.delayTimePostTrigger > 0) vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
            }
            if (onlyX[0] && !waitForAxes(sweep, onlyX, travelTimeMs(xFirst, xLast, sd.speed, sd.acceleration) * 2 + 1000))
            {
                if (sd.stopped) { st.aborted = true; return; }
                ++st.moveTimeouts;
            }
        }
    }

    // -------------------------------------------------------------------------
    // Coordinate list, stop-and-go: move, then one frame per light channel.
    // -------------------------------------------------------------------------
    static void coordinatesStopAndGo(StageScanningData &sd, ScanStats &st)
    {
        int seq[StageScanOrder::kLaserChannels + 1];
        const int nCh = StageScanOrder::channelSequence(sd.lightsourceIntensities, sd.ledarrayIntensity, seq);
        for (int i = 0; i < sd.coordinateCount; ++i)
        {
            if (sd.stopped) { st.aborted = true; return; }
            const StagePosition &p = sd.coordinates[i];
            const bool hasZ = (p.z != kKeepAxis);
            const int32_t target[3] = {p.x, p.y, hasZ ? p.z : 0};
            const bool active[3] = {true, true, hasZ};
            if (!moveTo(target, active, sd.speed, sd.acceleration))
            {
                if (sd.stopped) { st.aborted = true; return; }
                ++st.moveTimeouts;
            }
            for (int c = 0; c < nCh; ++c)
            {
                if (sd.stopped) { st.aborted = true; return; }
                exposeFrame(sd, seq[c], st.frames++);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Coordinate list, nonstop: head for the last coordinate and trigger when
    // the motion profile says each intermediate one is passed (as before).
    // -------------------------------------------------------------------------
    static void coordinatesNonstop(StageScanningData &sd, ScanStats &st)
    {
        MotorData *dx = FocusMotor::getData()[Stepper::X];
        MotorData *dy = FocusMotor::getData()[Stepper::Y];
        const int32_t sx = dx ? dx->currentPosition : 0;
        const int32_t sy = dy ? dy->currentPosition : 0;
        const StagePosition &last = sd.coordinates[sd.coordinateCount - 1];
        const int32_t target[3] = {last.x, last.y, 0};
        const bool xy[3] = {true, true, false};
        const uint32_t t0 = millis();
        dispatchMove(target, xy, sd.speed, sd.acceleration);
        for (int i = 0; i < sd.coordinateCount; ++i)
        {
            if (sd.stopped) { stopAxes(); st.aborted = true; return; }
            const uint32_t tx = travelTimeMs(sx, sd.coordinates[i].x, sd.speed, sd.acceleration);
            const uint32_t ty = travelTimeMs(sy, sd.coordinates[i].y, sd.speed, sd.acceleration);
            const uint32_t due = tx > ty ? tx : ty;
            const uint32_t elapsed = millis() - t0;
            if (due > elapsed) vTaskDelay(pdMS_TO_TICKS(due - elapsed));
            if (sd.delayTimePreTrigger > 0) vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
            fireTrigger(st.frames++, sd.delayTimeTrigger);
            if (sd.delayTimePostTrigger > 0) vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
        }
        const uint32_t tx = travelTimeMs(sx, last.x, sd.speed, sd.acceleration);
        const uint32_t ty = travelTimeMs(sy, last.y, sd.speed, sd.acceleration);
        if (!waitForAxes(target, xy, (tx > ty ? tx : ty) * 2 + 1000) && !sd.stopped) ++st.moveTimeouts;
    }

    // -------------------------------------------------------------------------
    // Entry point (runs in its own task, see MotorJsonParser::parseStageScan)
    // -------------------------------------------------------------------------
    void stageScan(bool isThread)
    {
        if (isRunning)
        {
            log_w("stagescan already running");
            if (isThread) vTaskDelete(NULL);
            return;
        }
        isRunning = true;
        StageScanningData &sd = stageScanningData;
        ScanStats st;

        FocusMotor::setEnable(true);
        if (pinConfig.CAMERA_TRIGGER_PIN >= 0)
        {
            pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
            digitalWrite(pinConfig.CAMERA_TRIGGER_PIN, pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW);
        }
        else
        {
            log_w("stagescan: no CAMERA_TRIGGER_PIN, only {\"cam\":1} notifications will be sent");
        }
        allLightsOff(sd);

        if (sd.useCoordinates && sd.coordinates != nullptr && sd.coordinateCount > 0)
        {
            log_i("stagescan: %d coordinates, nonstop=%d", sd.coordinateCount, (int)sd.nonstop);
            if (sd.nonstop) coordinatesNonstop(sd, st);
            else coordinatesStopAndGo(sd, st);
        }
        else
        {
            // A start of 0 means "from where the stage is" (what ImSwitch sends).
            MotorData *dx = FocusMotor::getData()[Stepper::X];
            MotorData *dy = FocusMotor::getData()[Stepper::Y];
            MotorData *dz = FocusMotor::getData()[Stepper::Z];
            const int32_t x0 = sd.xStart != 0 ? sd.xStart : (dx ? dx->currentPosition : 0);
            const int32_t y0 = sd.yStart != 0 ? sd.yStart : (dy ? dy->currentPosition : 0);
            const int32_t z0 = sd.zStart != 0 ? sd.zStart : (dz ? dz->currentPosition : 0);
            log_i("stagescan: grid %ux%ux%u from X:%d Y:%d Z:%d step X:%d Y:%d Z:%d nonstop=%d, %u frames",
                  (unsigned)sd.nX, (unsigned)sd.nY, (unsigned)sd.nZ, (int)x0, (int)y0, (int)z0,
                  (int)sd.xStep, (int)sd.yStep, (int)sd.zStep, (int)sd.nonstop,
                  (unsigned)StageScanOrder::frameCount(sd.nX, sd.nY, sd.nZ, sd.lightsourceIntensities, sd.ledarrayIntensity));
            if (sd.nonstop) gridNonstop(sd, x0, y0, z0, st);
            else gridStopAndGo(sd, x0, y0, z0, st);
        }

        allLightsOff(sd); // also covers an abort mid-exposure

        // Completion: {"stagescan":true,"frames":N,"moveTimeouts":k,"aborted":0|1,"qid":q,"success":0|1}
        // The host matches on "stagescan" + "success"; the counters let it
        // tell "all frames fired" from "stopped early" without guessing.
        cJSON *json = cJSON_CreateObject();
        cJsonTool::setJsonBool(json, "stagescan", 1);
        cJsonTool::setJsonInt(json, "frames", (int)st.frames);
        cJsonTool::setJsonInt(json, "moveTimeouts", (int)st.moveTimeouts);
        cJsonTool::setJsonInt(json, "aborted", st.aborted ? 1 : 0);
        cJsonTool::setJsonInt(json, keyQueueID, sd.qid);
        cJsonTool::setJsonInt(json, "success", (st.aborted || st.moveTimeouts) ? 0 : 1);
        sendJson(json);

        isRunning = false;
        if (isThread)
            vTaskDelete(NULL);
    }

    void stageScanThread(void *arg)
    // Grid:        {"task":"/motor_act","stagescan":{"xStart":0,"yStart":0,"zStart":0,"xStep":500,"yStep":500,"zStep":0,
    //               "nX":2,"nY":2,"nZ":1,"tPre":50,"tPost":50,"tTrig":1,"illumination":[0,100,0,0,0],"led":0}}
    // Coordinates: {"task":"/motor_act","stagescan":{"coordinates":[{"x":100,"y":200},{"x":300,"y":400,"z":10}],"tPre":50,"tPost":50}}
    {
        (void)arg;
        stageScan(true);
    }

} // namespace StageScan
