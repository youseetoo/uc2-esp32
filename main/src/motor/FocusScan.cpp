// FocusScan.cpp
#include "FocusScan.h"
#include "Arduino.h"
#include "cJSON.h"
#include "cJsonTool.h"

namespace FocusScan
{
    FocusScanningData focusScanningData;
    volatile bool isRunning = false;

    static inline void triggerOutput(int pin, int trigTime)
    {
        bool inv = pinConfig.CAMERA_TRIGGER_INVERTED;
        digitalWrite(pin, inv ? HIGH : LOW);
        ets_delay_us(trigTime);
        digitalWrite(pin, inv ? LOW : HIGH);
    }

    static inline void moveAbs(Stepper ax, int32_t pos,
                               int32_t speed = 20000,
                               int32_t acceleration = 1000000,
                               int32_t timeout = 2000)
    {
#if defined CAN_BUS_ENABLED && !defined CAN_RECEIVE_MOTOR
        auto *d = FocusMotor::getData()[ax];
        d->absolutePosition = 1;
        d->targetPosition = pos;
        d->speed = speed;
        d->acceleration = acceleration;
        d->isStop = 0;
        d->stopped = false;
        d->isforever = false;
        FocusMotor::startStepper(ax, 1);

        vTaskDelay(pdMS_TO_TICKS(100));
        uint32_t t0 = millis();
        while (!d->stopped && (millis() - t0) < uint32_t(timeout))
            vTaskDelay(1);
#endif
    }

    FocusScanningData *getFocusScanData() { return &focusScanningData; }

    void focusScanCAN(bool isThread)
    {
#if defined CAN_BUS_ENABLED && !defined CAN_RECEIVE_MOTOR
        auto &sd = focusScanningData;
        isRunning = true;

        int32_t posZ0 = FocusMotor::getData()[Stepper::Z]->currentPosition;
        if (sd.zStart)
            posZ0 = sd.zStart;

        pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
        digitalWrite(pinConfig.CAMERA_TRIGGER_PIN,
                     pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW);

        moveAbs(Stepper::Z, posZ0, sd.speed, sd.acceleration);

        for (uint16_t iz = 0; iz < sd.nZ && !sd.stopped; ++iz)
        {
            int32_t tgtZ = posZ0 + int32_t(iz) * sd.zStep;
            moveAbs(Stepper::Z, tgtZ, sd.speed, sd.acceleration);

#ifdef LED_CONTROLLER
            if (sd.ledarrayIntensity)
            {
                // TODO: Generalize this:
                LedCommand cmd;
                cmd.mode = LedMode::CIRCLE;
                cmd.r = sd.ledarrayIntensity; // pinConfig.JOYSTICK_MAX_ILLU;
                cmd.g = sd.ledarrayIntensity; // pinConfig.JOYSTICK_MAX_ILLU;
                cmd.b = sd.ledarrayIntensity; // pinConfig.JOYSTICK_MAX_ILLU;
                cmd.radius = 8;               // radius of the circle
                cmd.ledIndex = 0;             // not used
                cmd.region[0] = 0;            // not used
                cmd.qid = 0;                  // not used
                vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));

                vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));

                cmd.r = 0;
                cmd.g = 0;
                cmd.b = 0; // switch off the LED
                can_controller::sendLedCommandToCANDriver(cmd, pinConfig.CAN_ID_LED_0);
            }
#endif
#ifdef LASER_CONTROLLER
            for (uint8_t i = 0; i < 4; ++i)
            {
#if defined CAN_BUS_ENABLED && !defined CAN_RECEIVE_LASER
                if (sd.lightsourceIntensities[i])
                {
                    LaserData l;
                    l.LASERid = i;
                    l.LASERval = sd.lightsourceIntensities[i];
                    can_controller::sendLaserDataToCANDriver(l);
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                    triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                    if (sd.lightsourceIntensities[i])
                    {
                        LaserData l;
                        l.LASERid = i;
                        l.LASERval = 0;
                        can_controller::sendLaserDataToCANDriver(l);
                    }
                }
#endif
            }
#endif
        }

        moveAbs(Stepper::Z, posZ0, sd.speed, sd.acceleration, 0);

        cJSON *json = cJSON_CreateObject();
        cJSON *focusscan = cJSON_CreateObject();
        cJSON_AddItemToObject(json, "focusscan", focusscan);
        cJsonTool::setJsonInt(json, keyQueueID, sd.qid);
        cJsonTool::setJsonInt(json, "success", 1);
        Serial.println("++");
        char *ret = cJSON_PrintUnformatted(json);
        Serial.println(ret);
        cJSON_Delete(json);
        free(ret);
        Serial.println("--");

        isRunning = false;
        if (isThread)
            vTaskDelete(NULL);
#endif
    }

    void focusScanThread(void *arg) { focusScanCAN(true); }

} // namespace FocusScan
