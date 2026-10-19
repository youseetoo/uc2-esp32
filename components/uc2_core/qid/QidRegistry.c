// T4.1: QidRegistry — pure-C QID tracking (was a C++ namespace).
// The C++ FocusMotor dependency is hidden behind the focusmotor_qid_c.h shim
// (gated on MOTOR_CONTROLLER). Serial output uses the C serial_out_* API.
#include "QidRegistry.h"
#include "uc2_serial_out.h"
#include "cJsonTool.h"
#include "esp_log.h"
#include "esp_timer.h"
#ifdef MOTOR_CONTROLLER
#include "focusmotor_qid_c.h"
// MOTOR_AXIS_COUNT is a per-app #define (in the app's PinConfig.h, which is
// C++). The C QidRegistry can't include it, so default to the max (10).
// The loop bound is min(MOTOR_AXIS_COUNT, QID_MAX_PAUSED_AXES) so a high
// default is safe — out-of-range axes are no-ops in the focusmotor_* shim.
#ifndef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 10
#endif
#endif

// ── Weak defaults for the focusmotor_* shim ─────────────────────────────────
// The real implementation lives in uc2_motor (focusmotor_qid_c.cpp); uc2_core
// cannot depend on uc2_motor (circular), so the QidRegistry links against
// weak no-op defaults. When uc2_motor is in the build (MOTOR_CONTROLLER),
// the linker resolves to the real shim.
#ifdef MOTOR_CONTROLLER
__attribute__((weak)) int focusmotor_get_qid(int a) { (void)a; return -1; }
__attribute__((weak)) int focusmotor_is_stopped(int a) { (void)a; return 1; }
__attribute__((weak)) int32_t focusmotor_get_current_position(int a) { (void)a; return 0; }
__attribute__((weak)) int32_t focusmotor_get_target_position(int a) { (void)a; return 0; }
__attribute__((weak)) int32_t focusmotor_get_speed(int a) { (void)a; return 0; }
__attribute__((weak)) int32_t focusmotor_get_acceleration(int a) { (void)a; return 0; }
__attribute__((weak)) int focusmotor_get_absolute(int a) { (void)a; return 0; }
__attribute__((weak)) void focusmotor_stop_stepper(int a) { (void)a; }
__attribute__((weak)) void focusmotor_set_qid(int a, int q) { (void)a; (void)q; }
__attribute__((weak)) void focusmotor_set_target_position(int a, int32_t t) { (void)a; (void)t; }
__attribute__((weak)) void focusmotor_set_speed(int a, int32_t s) { (void)a; (void)s; }
__attribute__((weak)) void focusmotor_set_acceleration(int a, int32_t c) { (void)a; (void)c; }
__attribute__((weak)) void focusmotor_set_absolute(int a, int b) { (void)a; (void)b; }
__attribute__((weak)) void focusmotor_set_is_stop(int a, int s) { (void)a; (void)s; }
__attribute__((weak)) void focusmotor_set_stopped(int a, int s) { (void)a; (void)s; }
__attribute__((weak)) void focusmotor_set_isforever(int a, int f) { (void)a; (void)f; }
__attribute__((weak)) void focusmotor_start_stepper(int a, int r) { (void)a; (void)r; }
#endif

static const char *TAG = "QidRegistry";

static QidEntry entries[QID_REGISTRY_SIZE];
static SemaphoreHandle_t xQidMutex = NULL;

void qid_setup(void)
{
    xQidMutex = xSemaphoreCreateMutex();
    for (int i = 0; i < QID_REGISTRY_SIZE; i++)
    {
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
static int findFreeSlot(void)
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

    serial_out_safe_send_json_string(jsonString);
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

int qid_register(int qid, uint8_t numActions, uint32_t timeoutMs)
{
    if (qid <= 0 || numActions == 0)
        return 0;

    if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
        return 0;

    // Check if qid already exists (re-registration replaces)
    int idx = findEntry(qid);
    if (idx < 0)
        idx = findFreeSlot();

    if (idx < 0)
    {
        xSemaphoreGive(xQidMutex);
        ESP_LOGW(TAG, "QidRegistry full, cannot register qid %d", qid);
        return 0;
    }

    entries[idx].qid = qid;
    entries[idx].totalActions = numActions;
    entries[idx].doneActions = 0;
    entries[idx].state = QID_BUSY;
    entries[idx].registeredAt = (uint32_t)(esp_timer_get_time() / 1000);
    entries[idx].timeoutMs = timeoutMs;
    entries[idx].suppressDoneReport = 0;
    for (int i = 0; i < QID_MAX_PAUSED_AXES; i++)
    {
        entries[idx].pausedAxes[i].paused = 0;
    }

    xSemaphoreGive(xQidMutex);
    ESP_LOGI(TAG, "QidRegistry: registered qid=%d with %d actions", qid, numActions);
    return 1;
}

void qid_report_action_done(int qid)
{
    ESP_LOGI(TAG, "QidRegistry: reportActionDone called for qid=%d", qid);
    if (qid <= 0) {
        ESP_LOGW(TAG, "QidRegistry: reportActionDone called with invalid qid=%d", qid);
        return;
    }
    if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
    {
        ESP_LOGW(TAG, "QidRegistry: reportActionDone called but mutex not available for qid=%d", qid);
        return;
    }

    int idx = findEntry(qid);
    if (idx < 0 || entries[idx].state != QID_BUSY)
    {
        xSemaphoreGive(xQidMutex);
        ESP_LOGW(TAG, "QidRegistry: reportActionDone called but qid=%d not found or not busy", qid);
        return;
    }

    if (entries[idx].suppressDoneReport)
    {
        // Pause in progress - don't count this as done
        xSemaphoreGive(xQidMutex);
        ESP_LOGI(TAG, "QidRegistry: reportActionDone for qid=%d suppressed due to pause", qid);
        return;
    }

    entries[idx].doneActions++;
    ESP_LOGI(TAG, "QidRegistry: qid=%d action done (%d/%d)", qid, entries[idx].doneActions, entries[idx].totalActions);

    if (entries[idx].doneActions >= entries[idx].totalActions)
    {
        entries[idx].state = QID_DONE;
        xSemaphoreGive(xQidMutex);
        sendQidNotification(qid, "done");
        ESP_LOGI(TAG, "QidRegistry: qid=%d completed", qid);
        return;
    }

    xSemaphoreGive(xQidMutex);
}

void qid_report_action_error(int qid)
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

QidState qid_get_state(int qid)
{
    if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(200)))
        return QID_FREE;

    int idx = findEntry(qid);
    QidState state = (idx >= 0) ? entries[idx].state : QID_FREE;
    xSemaphoreGive(xQidMutex);
    return state;
}

int qid_pause(int qid)
{
    if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
        return 0;

    int idx = findEntry(qid);
    if (idx < 0 || entries[idx].state != QID_BUSY)
    {
        xSemaphoreGive(xQidMutex);
        return 0;
    }

    // Suppress done reports during pause stop
    entries[idx].suppressDoneReport = 1;
#ifdef MOTOR_CONTROLLER
    // Stop all motors that belong to this QID and save their remaining steps
    for (int i = 0; i < MOTOR_AXIS_COUNT && i < QID_MAX_PAUSED_AXES; i++)
    {
        if (focusmotor_get_qid(i) == qid && !focusmotor_is_stopped(i))
        {
            xSemaphoreGive(xQidMutex);

            // Save remaining distance before stopping
            int32_t currentPos = focusmotor_get_current_position(i);
            int32_t targetPos = focusmotor_get_target_position(i);
            entries[idx].pausedAxes[i].remainingSteps = targetPos - currentPos;
            entries[idx].pausedAxes[i].speed = focusmotor_get_speed(i);
            entries[idx].pausedAxes[i].acceleration = focusmotor_get_acceleration(i);
            entries[idx].pausedAxes[i].isAbsolute = focusmotor_get_absolute(i);
            entries[idx].pausedAxes[i].originalTarget = targetPos;
            entries[idx].pausedAxes[i].paused = 1;

            // Stop the motor
            focusmotor_stop_stepper(i);
            if (!xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
                return 0;
            idx = findEntry(qid); // re-find after releasing mutex
            if (idx < 0)
            {
                xSemaphoreGive(xQidMutex);
                return 0;
            }
        }
    }
#endif
    entries[idx].state = QID_PAUSED;
    entries[idx].suppressDoneReport = 0;
    xSemaphoreGive(xQidMutex);

    sendQidNotification(qid, "paused");
    ESP_LOGI(TAG, "QidRegistry: qid=%d paused", qid);
    return 1;
}

int qid_resume(int qid)
{
    if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(500)))
        return 0;

    int idx = findEntry(qid);
    if (idx < 0 || entries[idx].state != QID_PAUSED)
    {
        xSemaphoreGive(xQidMutex);
        return 0;
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
            focusmotor_set_qid(i, qid);
            focusmotor_set_target_position(i, entries[idx].pausedAxes[i].remainingSteps);
            focusmotor_set_speed(i, entries[idx].pausedAxes[i].speed);
            focusmotor_set_acceleration(i, entries[idx].pausedAxes[i].acceleration);
            focusmotor_set_absolute(i, 0); // Remaining is always relative
            focusmotor_set_is_stop(i, 0);
            focusmotor_set_stopped(i, 0);
            focusmotor_set_isforever(i, 0);
            entries[idx].pausedAxes[i].paused = 0;
        }
    }
#endif

    // Update total/done counters for resumed movement
    entries[idx].totalActions = resumedAxes;
    entries[idx].doneActions = 0;
    entries[idx].registeredAt = (uint32_t)(esp_timer_get_time() / 1000); // Reset timeout
    entries[idx].suppressDoneReport = 0;

    xSemaphoreGive(xQidMutex);
#ifdef MOTOR_CONTROLLER
    // Start the motors (outside mutex)
    for (int i = 0; i < MOTOR_AXIS_COUNT && i < QID_MAX_PAUSED_AXES; i++)
    {
        if (focusmotor_get_qid(i) == qid && !focusmotor_is_stopped(i))
        {
            focusmotor_start_stepper(i, 0);
        }
    }
#endif

    ESP_LOGI(TAG, "QidRegistry: qid=%d resumed with %d axes", qid, resumedAxes);
    return 1;
}

void qid_tick_timeout(void)
{
    if (xQidMutex == NULL || !xSemaphoreTake(xQidMutex, pdMS_TO_TICKS(100)))
        return;

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
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
                ESP_LOGW(TAG, "QidRegistry: qid=%d timed out", qid);
                return; // Process one timeout per tick to avoid mutex issues
            }
        }
    }
    xSemaphoreGive(xQidMutex);
}

int qid_handle_state_query(cJSON *doc)
{
    int qid = cjson_tool_get_int(doc, "qid", 0);
    QidState state = qid_get_state(qid);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return -1;
    cJSON_AddNumberToObject(root, "qid", qid);
    cJSON_AddStringToObject(root, "state", stateToString(state));

    char *jsonString = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (jsonString == NULL)
        return -1;

    serial_out_safe_send_json_string(jsonString);
    free(jsonString);
    return qid;
}

int qid_handle_pause(cJSON *doc)
{
    int qid = cjson_tool_get_int(doc, "qid", 0);
    int ok = qid_pause(qid);
    return ok ? qid : -1;
}

int qid_handle_resume(cJSON *doc)
{
    int qid = cjson_tool_get_int(doc, "qid", 0);
    int ok = qid_resume(qid);
    return ok ? qid : -1;
}
