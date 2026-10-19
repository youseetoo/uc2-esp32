#include <PinConfig.h>
#include "StrobeSweep.h"
#include "StrobeSweepLogic.h"
#include "SyncLatch.h"
#include "FocusMotor.h"
#include "StageScan.h"
#include "Arduino.h"
#include "../../cJsonTool.h"
#include "../canopen/DeviceRouter.h"
#include "../canopen/RoutingTable.h"
#include "../serial/SerialProcess.h"
#include "../laser/PulseOut.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#ifdef LASER_CONTROLLER
#include "../laser/StrobeRoute.h"
#include "../laser/LaserStrobe.h"
#endif
#ifdef CAN_CONTROLLER_CANOPEN
#include "../canopen/SyncHook.h"
#include "../canopen/CANopenModule.h"
#include "../canopen/UC2_OD_Indices.h"
#endif

namespace StrobeSweep
{
    namespace L = StrobeSweepLogic;

    static const int kCameraTimerGroup = 1;
    static const int kCameraTimerIndex = 1;

    struct PosEntry
    {
        uint16_t n;
        int32_t x;
    };

    struct Outcome
    {
        const char *error = nullptr;
        bool aborted = false;
        uint32_t frames = 0;
        uint32_t positions = 0;
        bool laserUsed = false;
        bool strobeConfigured = false;
        bool latchOn = false;
        bool motorRemote = false;
        uint8_t motorNode = 0;
        uint8_t motorSub = 1;
    };

    static L::Params s_params;
    static volatile bool s_running = false;
    static volatile bool s_stop = false;
    static PulseOut s_camera;
    static QueueHandle_t s_posQueue = nullptr;
    static volatile uint32_t s_frameNo = 0;      // frames triggered (TX success or local tick)
    static volatile uint32_t s_cameraPulses = 0; // pulses actually started
    static volatile bool s_laserLocal = false;
    static volatile bool s_latchLocal = false;
    static volatile int s_axis = 1;
    static volatile uint8_t s_motorNode = 0;
    static volatile uint32_t s_trigUs = 100;
    static char s_reportBuf[1400];

    bool isRunning() { return s_running; }
    void stop() { s_stop = true; }

    static void emit(cJSON *j)
    {
        char *s = cJSON_PrintUnformatted(j);
        if (s)
        {
            SerialProcess::safeSendJsonString(s);
            free(s);
        }
        cJSON_Delete(j);
    }

    static void pushPosition(uint16_t n, int32_t x)
    {
        if (s_posQueue == nullptr)
            return;
        PosEntry e{n, x};
        xQueueSend(s_posQueue, &e, 0);
    }

    // One frame at trigger time t: camera pulse now, local flash after its
    // delay, local position latch. Runs in the CAN task (SYNC TX success) or
    // in the sweep task (all-local sweep).
    static void frameAt(int64_t t)
    {
        const uint32_t n = s_frameNo + 1;
        s_frameNo = n;
        if (s_camera.started() && s_camera.fire(0, s_trigUs))
            s_cameraPulses = s_cameraPulses + 1;
#ifdef LASER_CONTROLLER
        if (s_laserLocal)
            LaserStrobe::fireFrom(t);
#endif
        if (s_latchLocal)
            pushPosition((uint16_t)n, SyncLatch::livePosition(s_axis));
    }

#ifdef CAN_CONTROLLER_CANOPEN
    static void onSyncTx(int64_t t, void *)
    {
        frameAt(t);
    }

    static void onPosition(uint8_t node, int32_t pos, uint16_t count, void *)
    {
        if (node == s_motorNode)
            pushPosition(count, pos);
    }

    // Latch on/off on a motor node; enabling writes 0 first so the node
    // resets its count (it polls the object every millisecond).
    static bool setRemoteLatch(uint8_t node, uint8_t sub, bool en)
    {
        uint8_t v = 0;
        size_t n = 0;
        if (!CANopenModule::readSDO(node, UC2_OD::MOTOR_SYNC_LATCH_ENABLE, sub, &v, 1, &n))
            return false;
        if (en)
        {
            CANopenModule::writeSDO_u8(node, UC2_OD::MOTOR_SYNC_LATCH_ENABLE, sub, 0);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        CANopenModule::writeSDO_u8(node, UC2_OD::MOTOR_SYNC_LATCH_ENABLE, sub, en ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        v = 0;
        return CANopenModule::readSDO(node, UC2_OD::MOTOR_SYNC_LATCH_ENABLE, sub, &v, 1, &n) && ((v != 0) == en);
    }
#endif

    static void motorJson(int axis, bool stopCmd, int32_t target, int32_t speed, int32_t accel)
    {
        cJSON *doc = cJSON_CreateObject();
        cJSON *motor = cJSON_AddObjectToObject(doc, "motor");
        cJSON *steppers = cJSON_AddArrayToObject(motor, "steppers");
        cJSON *s = cJSON_CreateObject();
        cJSON_AddNumberToObject(s, "stepperid", axis);
        if (stopCmd)
        {
            cJSON_AddNumberToObject(s, "isStop", 1);
        }
        else
        {
            cJSON_AddNumberToObject(s, "position", target);
            cJSON_AddNumberToObject(s, "speed", speed);
            if (accel > 0)
                cJSON_AddNumberToObject(s, "acceleration", accel);
            cJSON_AddNumberToObject(s, "isabs", 1);
            cJSON_AddNumberToObject(s, "isforever", 0);
        }
        cJSON_AddItemToArray(steppers, s);
        cJSON *resp = DeviceRouter::handleMotorAct(doc);
        if (resp)
            cJSON_Delete(resp);
        cJSON_Delete(doc);
    }

    static bool motorArrived(int axis, int32_t target)
    {
        MotorData *d = FocusMotor::getData()[axis];
        if (d == nullptr)
            return true;
        const int32_t err = d->currentPosition - target;
        return d->stopped && err >= -1 && err <= 1;
    }

    static void flushBatch(L::ReportBatch &b)
    {
        if (b.count == 0)
            return;
        if (b.format(s_reportBuf, sizeof(s_reportBuf)) > 0)
            SerialProcess::safeSendJsonString(s_reportBuf);
        b.clear();
    }

    static void drain(L::ReportBatch &b, int batchSize, Outcome &o)
    {
        PosEntry e;
        while (s_posQueue != nullptr && xQueueReceive(s_posQueue, &e, 0) == pdTRUE)
        {
            o.positions++;
            if (b.push(e.n, e.x, batchSize))
                flushBatch(b);
        }
    }

    static void runSweep(const L::Params &p, Outcome &o)
    {
        const UC2::RouteEntry *mr = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, (uint8_t)p.axis);
        if (mr == nullptr || mr->where == UC2::RouteEntry::OFF)
        {
            o.error = "axis has no route";
            return;
        }
        o.motorRemote = mr->where == UC2::RouteEntry::REMOTE;
        o.motorNode = mr->nodeId;
        o.motorSub = (uint8_t)(mr->subAxis + 1);

        bool laserRemote = false;
        o.laserUsed = p.laser >= 0;
        if (o.laserUsed)
        {
#ifdef LASER_CONTROLLER
            const UC2::RouteEntry *lr = UC2::RoutingTable::find(UC2::RouteEntry::LASER, (uint8_t)p.laser);
            if (lr == nullptr || lr->where == UC2::RouteEntry::OFF)
            {
                o.error = "laser id has no route";
                return;
            }
            laserRemote = lr->where == UC2::RouteEntry::REMOTE;
#else
            o.error = "firmware built without laser support";
            return;
#endif
        }

        const bool useCan = L::needsCanSync(o.laserUsed, laserRemote, p.latch, o.motorRemote);
#ifndef CAN_CONTROLLER_CANOPEN
        if (useCan)
        {
            o.error = "remote laser or axis needs CANopen";
            return;
        }
#endif

#ifdef LASER_CONTROLLER
        if (o.laserUsed && p.delayUs >= 0)
        {
            StrobeRoute::Result r = StrobeRoute::configure(p.laser, true, (uint32_t)p.delayUs, (uint32_t)p.widthUs);
            if (!r.ok)
            {
                o.error = r.error ? r.error : "strobe configuration failed";
                return;
            }
            o.strobeConfigured = true;
        }
        s_laserLocal = o.laserUsed && !laserRemote;
#endif

        if (p.latch)
        {
            if (o.motorRemote)
            {
#ifdef CAN_CONTROLLER_CANOPEN
                o.latchOn = setRemoteLatch(o.motorNode, o.motorSub, true);
                if (!o.latchOn)
                    log_w("strobesweep: motor node 0x%02X has no position latch (firmware too old); positions not reported",
                          o.motorNode);
#endif
            }
            else
            {
                o.latchOn = true;
                s_latchLocal = true;
            }
        }

        if (pinConfig.CAMERA_TRIGGER_PIN >= 0)
        {
            if (!s_camera.begin(kCameraTimerGroup, kCameraTimerIndex, pinConfig.CAMERA_TRIGGER_PIN,
                                !pinConfig.CAMERA_TRIGGER_INVERTED))
                log_e("strobesweep: camera trigger timer unavailable");
        }
        else
        {
            log_w("strobesweep: no CAMERA_TRIGGER_PIN, the camera is not triggered");
        }

        if (s_posQueue == nullptr)
            s_posQueue = xQueueCreate(128, sizeof(PosEntry));
        else
            xQueueReset(s_posQueue);

#ifdef CAN_CONTROLLER_CANOPEN
        if (useCan)
        {
            SyncHook::addTxCallback(onSyncTx, nullptr);
            if (o.motorRemote && o.latchOn)
            {
                s_motorNode = o.motorNode;
                SyncHook::setPositionCallback(onPosition, nullptr);
            }
        }
#endif

        MotorData *d = FocusMotor::getData()[p.axis];
        const int32_t start = d ? d->currentPosition : 0;
        if (d && p.target != start)
        {
            d->stopped = false; // completion is the running -> stopped edge at the target
            d->isStop = 0;
            motorJson(p.axis, false, p.target, p.speed, p.accel);
        }

        log_i("strobesweep: axis %d %ld -> %ld at %ld steps/s, period %lu us, laser %d (%s), latch %d (%s), %s",
              p.axis, (long)start, (long)p.target, (long)p.speed, (unsigned long)p.periodUs, p.laser,
              laserRemote ? "remote" : "local", (int)o.latchOn, o.motorRemote ? "remote" : "local",
              useCan ? "SYNC over CAN" : "local trigger");

        L::ReportBatch batch;
        const int64_t t0 = esp_timer_get_time();
        const int64_t deadline = t0 + L::timeoutUs(p.maxFrames, p.periodUs, p.target - start, p.speed);
        int64_t next = t0;
        for (;;)
        {
            const int64_t now = esp_timer_get_time();
            const L::Exit e = L::exitReason(s_stop, p.maxFrames, o.frames, motorArrived(p.axis, p.target), now > deadline);
            if (e == L::Exit::Stopped)
            {
                o.aborted = true;
                break;
            }
            if (e == L::Exit::Timeout)
            {
                o.error = "timeout before the axis reached the target";
                break;
            }
            if (e != L::Exit::Continue)
                break;

            if (now >= next)
            {
#ifdef CAN_CONTROLLER_CANOPEN
                if (useCan)
                {
                    if (SyncHook::sendSync())
                        o.frames++;
                }
                else
#endif
                {
                    frameAt(now);
                    o.frames++;
                }
                next = L::nextTick(next, now, p.periodUs);
            }
            drain(batch, p.report, o);
            vTaskDelay(1);
        }

        if (o.aborted)
            motorJson(p.axis, true, 0, 0, 0);

        // Last SYNC still leaving, last TPDO3 still arriving.
        const int64_t tailEnd = esp_timer_get_time() + 50000;
        while (esp_timer_get_time() < tailEnd)
        {
            drain(batch, p.report, o);
            vTaskDelay(1);
        }
        flushBatch(batch);
    }

    static void sweepTask(void *)
    {
        const L::Params p = s_params;
        Outcome o;
        s_frameNo = 0;
        s_cameraPulses = 0;
        s_laserLocal = false;
        s_latchLocal = false;
        s_axis = p.axis;
        s_motorNode = 0;
        s_trigUs = p.trigUs;

        runSweep(p, o);

#ifdef CAN_CONTROLLER_CANOPEN
        SyncHook::removeTxCallback(onSyncTx, nullptr);
        SyncHook::setPositionCallback(nullptr, nullptr);
#endif
        s_laserLocal = false;
        s_latchLocal = false;

        long flashes = -1;
#ifdef LASER_CONTROLLER
        if (o.strobeConfigured)
        {
            StrobeRoute::Result r = StrobeRoute::configure(p.laser, false, 0, 0);
            flashes = (long)r.count;
            if (!r.ok && o.error == nullptr)
                o.error = r.error;
        }
        else if (o.laserUsed)
        {
            StrobeRoute::Result r = StrobeRoute::status(p.laser);
            if (r.ok)
                flashes = (long)r.count;
        }
#endif
#ifdef CAN_CONTROLLER_CANOPEN
        if (o.motorRemote && o.latchOn)
            setRemoteLatch(o.motorNode, o.motorSub, false);
#endif
        s_camera.end();

        cJSON *j = cJSON_CreateObject();
        cJsonTool::setJsonBool(j, "strobesweep", 1);
        cJsonTool::setJsonInt(j, "frames", (int)o.frames);
        cJsonTool::setJsonInt(j, "camera", (int)s_cameraPulses);
        cJsonTool::setJsonInt(j, "flashes", (int)flashes);
        cJsonTool::setJsonInt(j, "positions", (int)o.positions);
        cJsonTool::setJsonInt(j, "latch", o.latchOn ? 1 : 0);
        cJsonTool::setJsonInt(j, "strobe", (o.strobeConfigured || o.laserUsed) ? 1 : 0);
        cJsonTool::setJsonInt(j, "aborted", o.aborted ? 1 : 0);
        cJsonTool::setJsonInt(j, "success", L::success(o.aborted, o.error != nullptr, s_cameraPulses) ? 1 : 0);
        cJsonTool::setJsonInt(j, "qid", p.qid);
        if (o.error)
            cJSON_AddStringToObject(j, "error", o.error);
        emit(j);
        log_i("strobesweep done: %lu frames, %lu camera pulses, %ld flashes, %lu positions%s%s",
              (unsigned long)o.frames, (unsigned long)s_cameraPulses, flashes, (unsigned long)o.positions,
              o.error ? ", error: " : "", o.error ? o.error : "");

        s_running = false;
        vTaskDelete(NULL);
    }

    static void rejectNow(const char *error, int qid)
    {
        cJSON *j = cJSON_CreateObject();
        cJsonTool::setJsonBool(j, "strobesweep", 1);
        cJsonTool::setJsonInt(j, "frames", 0);
        cJsonTool::setJsonInt(j, "success", 0);
        cJsonTool::setJsonInt(j, "aborted", 0);
        cJsonTool::setJsonInt(j, "qid", qid);
        cJSON_AddStringToObject(j, "error", error);
        emit(j);
    }

    void parseJson(cJSON *doc)
    {
        cJSON *o = cJSON_GetObjectItem(doc, "strobesweep");
        if (o == nullptr || !cJSON_IsObject(o))
            return;
        if (cJsonTool::getJsonInt(o, "stopped", 0))
        {
            stop();
            return;
        }

        L::Params p;
        p.axis = cJsonTool::getJsonInt(o, "axis", 1);
        p.target = cJsonTool::getJsonInt(o, "target", 0);
        p.speed = cJsonTool::getJsonInt(o, "speed", 20000);
        p.accel = cJsonTool::getJsonInt(o, "acceleration", 0);
        p.periodUs = (uint32_t)max(0, cJsonTool::getJsonInt(o, "periodUs", 33333));
        p.trigUs = (uint32_t)max(0, cJsonTool::getJsonInt(o, "trigUs", (int)pinConfig.STROBE_TRIGGER_US));
        p.laser = cJsonTool::getJsonInt(o, "laser", -1);
        p.delayUs = cJsonTool::getJsonInt(o, "delayUs", -1);
        p.widthUs = cJsonTool::getJsonInt(o, "widthUs", -1);
        p.latch = cJsonTool::getJsonInt(o, "latch", 1) != 0;
        p.report = cJsonTool::getJsonInt(o, "report", 16);
        p.maxFrames = (uint32_t)max(0, cJsonTool::getJsonInt(o, "maxFrames", 0));
        p.qid = cJsonTool::getJsonInt(o, "qid", cJsonTool::getJsonInt(doc, "qid", -1));

        const char *err = L::validate(p);
        if (err == nullptr && (s_running || StageScan::isRunning))
            err = "busy: a strobe sweep or stage scan is running";
        if (err)
        {
            log_w("strobesweep rejected: %s", err);
            rejectNow(err, p.qid);
            return;
        }
        s_params = p;
        s_stop = false;
        s_running = true;
        if (xTaskCreate(sweepTask, "strobeSweep", 6144, NULL, 3, NULL) != pdPASS)
        {
            s_running = false;
            rejectNow("could not start the sweep task", p.qid);
        }
    }
}
