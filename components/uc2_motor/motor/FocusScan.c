// T4.1: pure C (was the C++ FocusScan namespace).
#include "FocusScan.h"
#include "uc2_hal/uc2_time.h"  // millis / delayMicroseconds (C)
#include "uc2_hal/uc2_gpio.h"  // pinMode / digitalWrite / HIGH / LOW / OUTPUT
#include "cJsonTool.h"       // cjson_tool_set_int
#include "uc2_serial_out.h"  // serial_out_safe_serialize_json (framed JSON out)
#include "JsonKeys.h"        // keyQueueID
#include <string.h>
#ifdef LASER_CONTROLLER
// laser_set_val(id, val): qid 0 + current despeckle — exactly what the
// stage-scan trigger pulse needs (the laser_set_val_c shim in FocusScan.h was
// never implemented in the T4.1 port).
#include "LaserController.h"
#endif

// NSDMI defaults (speed=20000, acceleration=1000000) as a C initializer; the
// rest is zero-initialized (stopped=false, lightsourceIntensities=0).
FocusScanningData focus_scanning_data = {
    .speed        = 20000,
    .acceleration = 1000000,
};
volatile bool focus_scan_is_running = false;

static inline void triggerOutput(int pin, int trigTime)
{
    bool inv = pinConfig.CAMERA_TRIGGER_INVERTED;
    digitalWrite(pin, inv ? HIGH : LOW);
    delayMicroseconds((uint32_t)trigTime);
    digitalWrite(pin, inv ? LOW : HIGH);
}

static inline void moveAbs(Stepper ax, int32_t pos,
                           int32_t speed,
                           int32_t acceleration,
                           int32_t timeout)
{
    MotorData *d = focusmotor_get_data()[ax];
    if (!d) return;
    d->absolutePosition = 1;
    d->targetPosition   = pos;
    d->speed            = speed;
    d->acceleration     = acceleration;
    d->isStop           = 0;
    d->stopped          = false;
    d->isforever        = false;
    focusmotor_start_stepper(ax, 1);

    vTaskDelay(pdMS_TO_TICKS(100));
    uint32_t t0 = millis();
    while (!d->stopped && (millis() - t0) < (uint32_t)timeout)
        vTaskDelay(1);
}

FocusScanningData *focus_scan_get_data(void) { return &focus_scanning_data; }

void focus_scan_can(bool isThread)
{
    FocusScanningData *sd = &focus_scanning_data;
    focus_scan_is_running = true;

    int32_t posZ0 = focusmotor_get_data()[Z]->currentPosition;
    if (sd->zStart)
        posZ0 = sd->zStart;

    pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
    digitalWrite(pinConfig.CAMERA_TRIGGER_PIN,
                 pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW);

    moveAbs(Z, posZ0, sd->speed, sd->acceleration, 2000);

    for (uint16_t iz = 0; iz < sd->nZ && !sd->stopped; ++iz)
    {
        int32_t tgtZ = posZ0 + (int32_t)iz * sd->zStep;
        moveAbs(Z, tgtZ, sd->speed, sd->acceleration, 2000);

#ifdef LED_CONTROLLER
        if (sd->ledarrayIntensity)
        {
            // Was: LedCommand{CIRCLE, r=g=b=intensity, radius=8, qid=0} via
            // LedController::execLedCommand — now the C shim in uc2_led.
            led_exec_circ_command(sd->ledarrayIntensity, 8, 0);
            vTaskDelay(pdMS_TO_TICKS(sd->delayTimePreTrigger));
            triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, (int)sd->delayTimeTrigger);
            vTaskDelay(pdMS_TO_TICKS(sd->delayTimePostTrigger));

            led_exec_circ_command(0, 8, 0);
        }
#endif
#ifdef LASER_CONTROLLER
        for (int i = 0; i < 4; ++i)
        {
            if (sd->lightsourceIntensities[i])
            {
                laser_set_val(i, sd->lightsourceIntensities[i]);
                vTaskDelay(pdMS_TO_TICKS(sd->delayTimePreTrigger));
                triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, (int)sd->delayTimeTrigger);
                vTaskDelay(pdMS_TO_TICKS(sd->delayTimePostTrigger));
                laser_set_val(i, 0);
            }
        }
#endif
    }

    moveAbs(Z, posZ0, sd->speed, sd->acceleration, 0);

    // Was: Serial.println("++"); cJSON_PrintUnformatted; Serial.println(ret);
    // free; Serial.println("--") — now the framed C output path.
    cJSON *json = cJSON_CreateObject();
    cJSON *focusscan = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "focusscan", focusscan);
    cjson_tool_set_int(json, keyQueueID, (int)sd->qid);
    cjson_tool_set_int(json, "success", 1);
    serial_out_safe_serialize_json(json);
    cJSON_Delete(json);

    focus_scan_is_running = false;
    if (isThread)
        vTaskDelete(NULL);
}

void focus_scan_thread(void *arg) { focus_scan_can(true); }
