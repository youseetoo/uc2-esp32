// T4.1: pure C header (was a C++ namespace with in-class initializers).
#pragma once
#include "PinConfigDefault.h"
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#include "MotorTypes.h"
#include "freertos/FreeRTOS.h"

#pragma pack(push,1)
struct HomeData
{
	uint8_t homeEndposPin;
	uint32_t homeTimeout; // ms
	int32_t homeSpeed;
	uint32_t homeMaxspeed;
	int8_t homeDirection;
	uint32_t homeTimeStarted;
	bool homeIsActive;
	uint8_t homeInEndposReleaseMode;  // Legacy field, kept for compatibility
	bool homeEndStopPolarity; // normally open
	uint16_t qid; // -1 sentinel (0xFFFF), like the old C++ default
	bool precise; // Use encoder-based stall detection for homing
	// Latched outcome of the most recent homing run, so a CANopen slave can push
	// it to the master over TPDO (OD 0x2016): 0=idle/none, 2=done, 3=timeout.
	// Set in home_send_done(), cleared at the start of home_start().
	uint8_t homeResultCode;

	// New CNC-style homing fields
	uint8_t homingPhase;  // 0=release-home-endstop, 1=fast approach, 2..7=normal sequence,
	                       // 8..10=post-release safety, 11..12=offset+done,
	                       // 13=escape WRONG endstop, 14=wait for wrong-endstop release,
	                       // 15..17=final back-off OFF the endstop before zeroing
	uint16_t homeRetractDistance;  // Steps to retract after hitting endstop
	int32_t homeFirstHitPosition;  // Position where endstop was first triggered
	int32_t homeEndOffset;  // Final position offset from home (can be positive or negative)
	TaskHandle_t homingTaskHandle;  // FreeRTOS task handle
};
typedef struct HomeData HomeData;  // C shorthand (C++ TUs keep working)
#pragma pack(pop)

// C replacement for the removed C++ default member initializers — call after
// every (re)allocation of a HomeData.
void home_data_init(struct HomeData *d);

#pragma pack(push,1)
struct HomeState
{
	bool isHoming;
	bool isHomed;
	int homeInEndposReleaseMode;
	uint32_t currentPosition;
	int axis;
};
#pragma pack(pop)

#pragma pack(push,1)
struct StopHomeCommand
{
	uint8_t axis;       // Axis to stop homing on
	uint8_t reserved;   // Padding byte - ensures sizeof(StopHomeCommand) == 2
	                     // to prevent collision with 1-byte restart handler in CAN dispatch.
	                     // Without this, StopHomeCommand(axis=0) triggers esp_restart()!
};
#pragma pack(pop)

void processHomeLoop(void *p);
void homingTaskFunction(void *parameter);

#ifdef __cplusplus
extern "C" {
#endif

int home_act(cJSON *ob);
cJSON *home_get(cJSON *ob);
void home_setup(void);
void home_loop(void);
void home_check_and_process_home(Stepper s, int digitalin_val);
int home_parse_data(cJSON *doc);
void home_run_stepper(int s);
void home_start(int axis, int homeTimeout, int homeSpeed, int homeMaxspeed,
                int homeDirection, int homeEndStopPolarity, int homeEndOffset, int qid);
void home_stop(int axis);
struct HomeData **home_get_data(void);
void home_send_done(int axis, const char *status);

#ifdef __cplusplus
}
#endif
