#include "AxisNotify.h"
#include "AxisTypes.h"
#include "../config/RuntimeConfig.h"
#include "../serial/SerialProcess.h"
#include "cJSON.h"
#include "esp_log.h"

#ifdef CAN_CONTROLLER_CANOPEN
#include "../canopen/CANopenModule.h"
#endif

namespace AxisNotify
{
    // Small lock-free-ish ring buffer. Producers (fault sites) run in the motor
    // loop / high-priority task; the drain runs in the main loop. A short crit
    // section keeps head/tail consistent without blocking on CAN.
    static constexpr int QUEUE_LEN = 8;
    static Event s_queue[QUEUE_LEN];
    static volatile int s_head = 0; // next write
    static volatile int s_tail = 0; // next read
    static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

    uint16_t emcyCodeForFault(uint8_t fault)
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

    const char *faultName(uint8_t fault)
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

    void report(const Event &e)
    {
        portENTER_CRITICAL(&s_mux);
        int next = (s_head + 1) % QUEUE_LEN;
        if (next != s_tail) // not full
        {
            s_queue[s_head] = e;
            s_head = next;
        }
        portEXIT_CRITICAL(&s_mux);
    }

    // Emit the {"axisEvent":{...}} JSON on the local serial channel.
    static void emitJson(const Event &e)
    {
        cJSON *root = cJSON_CreateObject();
        cJSON *ev = cJSON_AddObjectToObject(root, "axisEvent");
        cJSON_AddNumberToObject(ev, "axis", e.axis);
        cJSON_AddStringToObject(ev, "fault", faultName(e.fault));
        cJSON_AddNumberToObject(ev, "posErrSteps", e.posErrSteps);
        cJSON_AddNumberToObject(ev, "commandedSteps", e.commandedSteps);
        cJSON_AddNumberToObject(ev, "measuredSteps", e.measuredSteps);
        char *json = cJSON_PrintUnformatted(root);
        if (json)
        {
            SerialProcess::safeSendJsonString(json);
            free(json);
        }
        cJSON_Delete(root);
    }

    void loop()
    {
        // Drain whatever is queued. Copy out under the lock, act outside it.
        while (true)
        {
            Event e;
            portENTER_CRITICAL(&s_mux);
            bool have = (s_tail != s_head);
            if (have)
            {
                e = s_queue[s_tail];
                s_tail = (s_tail + 1) % QUEUE_LEN;
            }
            portEXIT_CRITICAL(&s_mux);
            if (!have)
                break;

#ifdef CAN_CONTROLLER_CANOPEN
            if (runtimeConfig.isSlave())
            {
                // The host lives behind the master: raise an EMCY; the master
                // converts it to the JSON event.
                CANopenModule::emitAxisEmcy(e.axis, e.fault, e.posErrSteps);
                continue;
            }
#endif
            // Standalone (or a master with a local fault): emit JSON directly.
            emitJson(e);
        }
    }
}
