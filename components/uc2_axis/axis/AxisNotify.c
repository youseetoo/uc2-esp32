// T4.1: AxisNotify — pure C (was a C++ namespace).
static const char *TAG = "AxisNotify";
#include "AxisNotify.h"
#include "AxisTypes.h"
#include "RuntimeConfig.h"    // runtime_config_is_slave (C API)
#include "uc2_serial_out.h"   // serial_out_safe_send_json_string (C API)
#include "cJSON.h"
#include "uc2_hal/uc2_log.h"
#include "freertos/FreeRTOS.h" // portMUX
#include <stdlib.h>

#ifdef CONFIG_UC2_CAN_CONTROLLER_CANOPEN
// C wrapper implemented by CANopenModule.cpp (extern "C"); weak default so a
// build with the axis code but a different CAN configuration still links.
__attribute__((weak)) void canopen_module_emit_axis_emcy(uint8_t axis, uint8_t fault,
                                                         int32_t posErrSteps)
{
    (void)axis; (void)fault; (void)posErrSteps;
}
#endif

// Small lock-free-ish ring buffer. Producers (fault sites) run in the motor
// loop / high-priority task; the drain runs in the main loop. A short crit
// section keeps head/tail consistent without blocking on CAN.
#define AXIS_NOTIFY_QUEUE_LEN 8
static AxisEvent s_queue[AXIS_NOTIFY_QUEUE_LEN];
static volatile int s_head = 0; // next write
static volatile int s_tail = 0; // next read
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

uint16_t axis_notify_emcy_code_for_fault(uint8_t fault)
{
    // Manufacturer-specific EMCY error codes (0xFFxx range).
    switch (fault)
    {
    case FAULT_STALL:       return 0xFF01;
    case FAULT_LOST_STEPS:  return 0xFF02;
    case FAULT_DIVERGENCE:  return 0xFF03;
    case FAULT_TIMEOUT:     return 0xFF04;
    case FAULT_CAL_INVALID: return 0xFF05;
    case FAULT_CAL_FAILED:  return 0xFF06;
    case FAULT_ENC_NOISE:   return 0xFF07;
    default:                return 0xFF00;
    }
}

const char *axis_notify_fault_name(uint8_t fault)
{
    switch (fault)
    {
    case FAULT_STALL:       return "STALL";
    case FAULT_LOST_STEPS:  return "LOST_STEPS";
    case FAULT_DIVERGENCE:  return "DIVERGENCE";
    case FAULT_TIMEOUT:     return "TIMEOUT";
    case FAULT_CAL_INVALID: return "CAL_INVALID";
    case FAULT_CAL_FAILED:  return "CAL_FAILED";
    case FAULT_ENC_NOISE:   return "ENC_NOISE";
    default:                return "NONE";
    }
}

void axis_notify_report(const AxisEvent *e)
{
    portENTER_CRITICAL(&s_mux);
    int next = (s_head + 1) % AXIS_NOTIFY_QUEUE_LEN;
    if (next != s_tail) // not full
    {
        s_queue[s_head] = *e;
        s_head = next;
    }
    portEXIT_CRITICAL(&s_mux);
}

// Emit the {"axisEvent":{...}} JSON on the local serial channel.
static void axis_notify_emit_json(const AxisEvent *e)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *ev = cJSON_AddObjectToObject(root, "axisEvent");
    cJSON_AddNumberToObject(ev, "axis", e->axis);
    cJSON_AddStringToObject(ev, "fault", axis_notify_fault_name(e->fault));
    cJSON_AddNumberToObject(ev, "posErrSteps", e->posErrSteps);
    cJSON_AddNumberToObject(ev, "commandedSteps", e->commandedSteps);
    cJSON_AddNumberToObject(ev, "measuredSteps", e->measuredSteps);
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        serial_out_safe_send_json_string(json);
        free(json);
    }
    cJSON_Delete(root);
}

void axis_notify_loop(void)
{
    // Drain whatever is queued. Copy out under the lock, act outside it.
    while (true)
    {
        AxisEvent e;
        portENTER_CRITICAL(&s_mux);
        bool have = (s_tail != s_head);
        if (have)
        {
            e = s_queue[s_tail];
            s_tail = (s_tail + 1) % AXIS_NOTIFY_QUEUE_LEN;
        }
        portEXIT_CRITICAL(&s_mux);
        if (!have)
            break;

#ifdef CONFIG_UC2_CAN_CONTROLLER_CANOPEN
        if (runtime_config_is_slave(&runtimeConfig))
        {
            // The host lives behind the master: raise an EMCY; the master
            // converts it to the JSON event.
            canopen_module_emit_axis_emcy(e.axis, e.fault, e.posErrSteps);
            continue;
        }
#endif
        // Standalone (or a master with a local fault): emit JSON directly.
        axis_notify_emit_json(&e);
    }
}
