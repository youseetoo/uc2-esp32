#pragma once
// T4.1: QidRegistry converted from a C++ namespace to a pure-C module so it can
// be built by a C compiler and linked into C call sites. The C++ FocusMotor
// dependency (pause/resume motor control, gated on MOTOR_CONTROLLER) is hidden
// behind the focusmotor_qid_c.h shim.
#include <stdint.h>
#include <stdbool.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Maximum number of simultaneously tracked QIDs
#define QID_REGISTRY_SIZE 8
// Maximum number of motor axes stored per QID for pause/resume
#define QID_MAX_PAUSED_AXES 4
// Default timeout for a QID before it's marked as timed out (ms)
#define QID_DEFAULT_TIMEOUT_MS 30000

#ifdef __cplusplus
extern "C" {
#endif

// QID lifecycle state
typedef enum QidState {
    QID_FREE = 0,    // Slot is unused
    QID_BUSY,        // Action in progress
    QID_DONE,        // All actions completed
    QID_PAUSED,      // Paused by user
    QID_TIMEOUT,     // Timed out
    QID_ERROR        // Error occurred
} QidState;

// Pause data for a single motor axis
typedef struct PausedAxisData {
    uint8_t paused;
    int32_t remainingSteps;
    int32_t speed;
    int32_t acceleration;
    uint8_t isAbsolute;
    int32_t originalTarget;
} PausedAxisData;

// One tracked QID entry
typedef struct QidEntry {
    int qid;
    uint8_t totalActions;
    uint8_t doneActions;
    QidState state;
    uint32_t registeredAt;    // millis() timestamp
    uint32_t timeoutMs;
    uint8_t suppressDoneReport;  // For internal pause/stop without triggering done
    PausedAxisData pausedAxes[QID_MAX_PAUSED_AXES];
} QidEntry;

// Initialize the registry (call once in setup)
void qid_setup(void);

// Register a new QID with expected number of actions.
// Returns 1 if registered, 0 if registry full.
int qid_register(int qid, uint8_t numActions, uint32_t timeoutMs);

// Report that one action for this QID is done.
// When all actions done, automatically sends {"qid":X,"state":"done"}.
void qid_report_action_done(int qid);

// Report an error for this QID - sends error immediately.
void qid_report_action_error(int qid);

// Query the state of a QID.
QidState qid_get_state(int qid);

// Pause a QID - stops all associated motors, saves remaining steps.
// Returns 1 if paused successfully.
int qid_pause(int qid);

// Resume a paused QID - restarts motors with saved remaining steps.
// Returns 1 if resumed successfully.
int qid_resume(int qid);

// Check for timed-out QIDs - call from FocusMotor::loop().
void qid_tick_timeout(void);

// Process /qid_state JSON command. Returns the qid, or -1 on error.
int qid_handle_state_query(cJSON *doc);

// Process /qid_pause JSON command. Returns the qid, or -1 on error.
int qid_handle_pause(cJSON *doc);

// Process /qid_resume JSON command. Returns the qid, or -1 on error.
int qid_handle_resume(cJSON *doc);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#ifdef __cplusplus
// C++ convenience overload (C has no default arguments).
inline int qid_register(int qid, uint8_t numActions)
{ return qid_register(qid, numActions, QID_DEFAULT_TIMEOUT_MS); }
#endif // __cplusplus
