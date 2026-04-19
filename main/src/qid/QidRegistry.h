#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <PinConfig.h>
#include "cJSON.h"

// Maximum number of simultaneously tracked QIDs
#define QID_REGISTRY_SIZE 8
// Maximum number of motor axes stored per QID for pause/resume
#define QID_MAX_PAUSED_AXES 4
// Default timeout for a QID before it's marked as timed out (ms)
#define QID_DEFAULT_TIMEOUT_MS 30000

namespace QidRegistry
{
    enum QidState : uint8_t
    {
        QID_FREE = 0,    // Slot is unused
        QID_BUSY,        // Action in progress
        QID_DONE,        // All actions completed
        QID_PAUSED,      // Paused by user
        QID_TIMEOUT,     // Timed out
        QID_ERROR        // Error occurred
    };

    // Pause data for motor axes
    struct PausedAxisData
    {
        bool paused;
        int32_t remainingSteps;
        int32_t speed;
        int32_t acceleration;
        bool isAbsolute;
        int32_t originalTarget;
    };

    struct QidEntry
    {
        int qid;
        uint8_t totalActions;
        uint8_t doneActions;
        QidState state;
        uint32_t registeredAt;    // millis() timestamp
        uint32_t timeoutMs;
        bool suppressDoneReport;  // For internal pause/stop without triggering done
        PausedAxisData pausedAxes[QID_MAX_PAUSED_AXES];
    };

    // Initialize the registry (call once in setup)
    void setup();

    // Register a new QID with expected number of actions
    // Returns true if registered, false if registry full
    bool registerQid(int qid, uint8_t numActions, uint32_t timeoutMs = QID_DEFAULT_TIMEOUT_MS);

    // Report that one action for this QID is done
    // When all actions done, automatically sends {"qid":X,"state":"done"}
    void reportActionDone(int qid);

    // Report an error for this QID - sends error immediately
    void reportActionError(int qid);

    // Query the state of a QID
    QidState getQidState(int qid);

    // Pause a QID - stops all associated motors, saves remaining steps
    // Returns true if paused successfully
    bool pauseQid(int qid);

    // Resume a paused QID - restarts motors with saved remaining steps
    // Returns true if resumed successfully
    bool resumeQid(int qid);

    // Check for timed-out QIDs - call from FocusMotor::loop()
    void tickTimeout();

    // Process /qid_state JSON command
    int handleQidStateQuery(cJSON *doc);

    // Process /qid_pause JSON command
    int handleQidPause(cJSON *doc);

    // Process /qid_resume JSON command
    int handleQidResume(cJSON *doc);
};
