#include "QidRegistry.h"
#include "../serial/SerialProcess.h"
#include "../motor/FocusMotor.h"
#include "../../cJsonTool.h"
#include "esp_log.h"

#ifdef CAN_BUS_ENABLED
#include "../can/can_transport.h"
#endif

static const char *TAG = "QidRegistry";

namespace QidRegistry
{
    static QidEntry entries[QID_REGISTRY_SIZE];
    static SemaphoreHandle_t xQidMutex = NULL;

    void setup()
    {
        xQidMutex = xSemaphoreCreateMutex();
        for (int i = 0; i < QID_REGISTRY_SIZE; i++)
        {
            // Initialize all entries as free
            entries[i].state = QID_FREE;
            entries[i].qid = -1;
        }
    }

    // Find entry index by qid, returns -1 if not found
    static int findEntry(int qid)
    {
        for (int i = 0; i < QID_REGISTRY_SIZE; i++)
        {
            if (entries[i].qid == qid && entries[i].state != QID_FREE)
                return i;
        }
        return -1;
    }

    // Find a free slot, returns -1 if full
    static int findFreeSlot()
    {
        for (int i = 0; i < QID_REGISTRY_SIZE; i++)
        {
            if (entries[i].state == QID_FREE)
                return i;
        }
        // Evict oldest DONE/TIMEOUT/ERROR entry
        uint32_t oldestTime = UINT32_MAX;
        int oldestIdx = -1;
        for (int i = 0; i < QID_REGISTRY_SIZE; i++)
        {
            if (entries[i].state == QID_DONE || entries[i].state == QID_TIMEOUT || entries[i].state == QID_ERROR)
            {
                if (entries[i].registeredAt < oldestTime)
                {
                    oldestTime = entries[i].registeredAt;
                    oldestIdx = i;
                }
            }
        }
        return oldestIdx;
    }

    // Send QID state change notification over serial
    static void sendQidNotification(int qid, const char *stateStr)
    {
        cJSON *root = cJSON_CreateObject();
        if (root == NULL)
            return;
        cJSON_AddNumberToObject(root, "qid", qid);
        cJSON_AddStringToObject(root, "state", stateStr);

        char *jsonString = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (jsonString == NULL)
            return;

        SerialProcess::safeSendJsonString(jsonString);
        free(jsonString);
    }

    static const char *stateToString(QidState state)
    {
        switch (state)
        {
        case QID_BUSY:
            return "busy";
        case QID_DONE:
            return "done";
        case QID_PAUSED:
            return "paused";
        case QID_TIMEOUT:
            return "timeout";
        case QID_ERROR:
            return "error";
        default:
            return "unknown";
        }
    }

    bool registerQid(int qid, uint8_t numActions, uint32_t timeoutMs)
    {
        if (qid <= 0 || numActions == 0)
            return false;

        if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
            return false;

        // Check if qid already exists (re-registration replaces)
        int idx = findEntry(qid);
        if (idx < 0)
            idx = findFreeSlot();

        if (idx < 0)
        {
            xSemaphoreGive(xQidMutex);
            log_w("QidRegistry full, cannot register qid %d", qid);
            return false;
        }

        entries[idx].qid = qid;
        entries[idx].totalActions = numActions;
        entries[idx].doneActions = 0;
        entries[idx].state = QID_BUSY;
        entries[idx].registeredAt = millis();
        entries[idx].timeoutMs = timeoutMs;
        entries[idx].suppressDoneReport = false;
        for (int i = 0; i < QID_MAX_PAUSED_AXES; i++)
        {
            entries[idx].pausedAxes[i].paused = false;
        }

        xSemaphoreGive(xQidMutex);
        log_i("QidRegistry: registered qid=%d with %d actions", qid, numActions);
        return true;
    }

    void reportActionDone(int qid)
    {
        log_i("QidRegistry: reportActionDone called for qid=%d", qid);
        if (qid <= 0){
            log_w("QidRegistry: reportActionDone called with invalid qid=%d", qid);
            return;
        }
        if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
        {
            log_w("QidRegistry: reportActionDone called but mutex not available for qid=%d", qid);
            return;
        }

        int idx = findEntry(qid);
        if (idx < 0 || entries[idx].state != QID_BUSY)
        {
            xSemaphoreGive(xQidMutex);
            log_w("QidRegistry: reportActionDone called but qid=%d not found or not busy", qid);
            return;
        }

        if (entries[idx].suppressDoneReport)
        {
            // Pause in progress - don't count this as done
            xSemaphoreGive(xQidMutex);
            log_i("QidRegistry: reportActionDone for qid=%d suppressed due to pause", qid);
            return;
        }

        entries[idx].doneActions++;
        log_i("QidRegistry: qid=%d action done (%d/%d)", qid, entries[idx].doneActions, entries[idx].totalActions);

        if (entries[idx].doneActions >= entries[idx].totalActions)
        {
            entries[idx].state = QID_DONE;
            xSemaphoreGive(xQidMutex);
            sendQidNotification(qid, "done");
            log_i("QidRegistry: qid=%d completed", qid);
            return;
        }

        xSemaphoreGive(xQidMutex);
    }

    void reportActionError(int qid)
    {
        if (qid <= 0)
            return;
        if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
            return;

        int idx = findEntry(qid);
        if (idx >= 0)
        {
            entries[idx].state = QID_ERROR;
        }
        xSemaphoreGive(xQidMutex);
        sendQidNotification(qid, "error");
    }

    QidState getQidState(int qid)
    {
        if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(200)))
            return QID_FREE;

        int idx = findEntry(qid);
        QidState state = (idx >= 0) ? entries[idx].state : QID_FREE;
        xSemaphoreGive(xQidMutex);
        return state;
    }

    bool pauseQid(int qid)
    {
        if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
            return false;

        int idx = findEntry(qid);
        if (idx < 0 || entries[idx].state != QID_BUSY)
        {
            xSemaphoreGive(xQidMutex);
            return false;
        }

        // Suppress done reports during pause stop
        entries[idx].suppressDoneReport = true;
        #ifdef MOTOR_CONTROLLER
        // Stop all motors that belong to this QID and save their remaining steps
        for (int i = 0; i < MOTOR_AXIS_COUNT && i < QID_MAX_PAUSED_AXES; i++)
        {
            if (FocusMotor::getData()[i]->qid == qid && !FocusMotor::getData()[i]->stopped)
            {
                xSemaphoreGive(xQidMutex);

                #if defined(CAN_BUS_ENABLED) 
                // Save remaining distance before stopping
                int32_t currentPos = FocusMotor::getData()[i]->currentPosition;
                int32_t targetPos = FocusMotor::getData()[i]->targetPosition;
                entries[idx].pausedAxes[i].remainingSteps = targetPos - currentPos;
                entries[idx].pausedAxes[i].speed = FocusMotor::getData()[i]->speed;
                entries[idx].pausedAxes[i].acceleration = FocusMotor::getData()[i]->acceleration;
                entries[idx].pausedAxes[i].isAbsolute = FocusMotor::getData()[i]->absolutePosition;
                entries[idx].pausedAxes[i].originalTarget = targetPos;
                entries[idx].pausedAxes[i].paused = true;

                // Stop the motor
                FocusMotor::stopStepper(i);
#endif
                if (!xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
                    return false;
                idx = findEntry(qid); // re-find after releasing mutex
                if (idx < 0)
                {
                    xSemaphoreGive(xQidMutex);
                    return false;
                }
            }
        }
        
        #endif
        entries[idx].state = QID_PAUSED;
        entries[idx].suppressDoneReport = false;
        xSemaphoreGive(xQidMutex);

        sendQidNotification(qid, "paused");
        log_i("QidRegistry: qid=%d paused", qid);
        return true;
    }

    bool resumeQid(int qid)
    {
        if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
            return false;

        int idx = findEntry(qid);
        if (idx < 0 || entries[idx].state != QID_PAUSED)
        {
            xSemaphoreGive(xQidMutex);
            return false;
        }

        // Reset done counter - we restart the paused axes
        uint8_t resumedAxes = 0;
        entries[idx].state = QID_BUSY;

        #ifdef MOTOR_CONTROLLER
        for (int i = 0; i < MOTOR_AXIS_COUNT && i < QID_MAX_PAUSED_AXES; i++)
        {
            if (entries[idx].pausedAxes[i].paused)
            {
                resumedAxes++;
                // Set up motor data for remaining movement
                FocusMotor::getData()[i]->qid = qid;
                FocusMotor::getData()[i]->targetPosition = entries[idx].pausedAxes[i].remainingSteps;
                FocusMotor::getData()[i]->speed = entries[idx].pausedAxes[i].speed;
                FocusMotor::getData()[i]->acceleration = entries[idx].pausedAxes[i].acceleration;
                FocusMotor::getData()[i]->absolutePosition = false; // Remaining is always relative
                FocusMotor::getData()[i]->isStop = false;
                FocusMotor::getData()[i]->stopped = false;
                FocusMotor::getData()[i]->isforever = false;
                entries[idx].pausedAxes[i].paused = false;
            }
        }
        #endif

        // Update total/done counters for resumed movement
        entries[idx].totalActions = resumedAxes;
        entries[idx].doneActions = 0;
        entries[idx].registeredAt = millis(); // Reset timeout
        entries[idx].suppressDoneReport = false;

        xSemaphoreGive(xQidMutex);
        #ifdef MOTOR_CONTROLLER
        // Start the motors (outside mutex)
        for (int i = 0; i < MOTOR_AXIS_COUNT && i < QID_MAX_PAUSED_AXES; i++)
        {
            if (FocusMotor::getData()[i]->qid == qid && !FocusMotor::getData()[i]->stopped)
            {
                FocusMotor::startStepper(i, 0);
            }
        }
        #endif

        log_i("QidRegistry: qid=%d resumed with %d axes", qid, resumedAxes);
        return true;
    }

    void tickTimeout()
    {
        if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(100)))
            return;

        uint32_t now = millis();
        for (int i = 0; i < QID_REGISTRY_SIZE; i++)
        {
            if (entries[i].state == QID_BUSY && entries[i].timeoutMs > 0)
            {
                if ((now - entries[i].registeredAt) > entries[i].timeoutMs)
                {
                    int qid = entries[i].qid;
                    entries[i].state = QID_TIMEOUT;
                    xSemaphoreGive(xQidMutex);
                    sendQidNotification(qid, "timeout");
                    log_w("QidRegistry: qid=%d timed out", qid);
                    return; // Process one timeout per tick to avoid mutex issues
                }
            }
        }
        xSemaphoreGive(xQidMutex);
    }

    int handleQidStateQuery(cJSON *doc)
    {
        int qid = cJsonTool::getJsonInt(doc, "qid");
        QidState state = getQidState(qid);

        cJSON *root = cJSON_CreateObject();
        if (root == NULL)
            return -1;
        cJSON_AddNumberToObject(root, "qid", qid);
        cJSON_AddStringToObject(root, "state", stateToString(state));

        char *jsonString = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        if (jsonString == NULL)
            return -1;

        SerialProcess::safeSendJsonString(jsonString);
        free(jsonString);
        return qid;
    }

    int handleQidPause(cJSON *doc)
    {
        int qid = cJsonTool::getJsonInt(doc, "qid");
        bool ok = pauseQid(qid);
        return ok ? qid : -1;
    }

    int handleQidResume(cJSON *doc)
    {
        int qid = cJsonTool::getJsonInt(doc, "qid");
        bool ok = resumeQid(qid);
        return ok ? qid : -1;
    }

} // namespace QidRegistry
