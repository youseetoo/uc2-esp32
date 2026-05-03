#include "BuzzerController.h"
#include <Arduino.h>
#include <string.h>

namespace BuzzerController
{
    static const char *TAG = "BuzzerController";

    static const uint8_t LEDC_CHANNEL = 7;       // dedicated channel for the buzzer
    static const uint8_t LEDC_RES_BITS = 8;       // 8-bit resolution -> duty 0..255

    static bool initialized = false;
    static bool toneActive = false;
    static uint32_t toneStopAtMs = 0;

    // Non-blocking beep sequence
    static uint8_t  seqRemaining = 0;
    static uint32_t seqFreq = 0;
    static uint32_t seqOnMs = 0;
    static uint32_t seqOffMs = 0;
    static uint32_t seqNextAtMs = 0;
    static bool     seqInOnPhase = false;

    static void hwOff()
    {
        if (!initialized) return;
        ledcWrite(LEDC_CHANNEL, 0);
        toneActive = false;

    }

    static void hwTone(uint32_t freq)
    {
        if (!initialized || freq == 0)
        {
            hwOff();
            return;
        }
        log_i("hwTone: freq=%d Hz", freq);
        // Use channel-based API (Arduino 2.0.x / ESP-IDF): ledcWriteTone takes a channel, not a pin.
        ledcWriteTone(LEDC_CHANNEL, freq);
        ledcWrite(LEDC_CHANNEL, 1 << (LEDC_RES_BITS - 1)); // 50% duty
        toneActive = true;
    }

    void setup()
    {
        if (pinConfig.BUZZER_PIN < 0)
        {
            log_i("BuzzerController: BUZZER_PIN disabled");
            return;
        }
        // https://docs.sunfounder.com/projects/esp32-starter-kit/de/latest/arduino/basic_projects/ar_pa_buz.html
        //ledcAttach(pinConfig.BUZZER_PIN, 2000, 8); // Set up the PWM pin
        
        ledcSetup(LEDC_CHANNEL, 2000, LEDC_RES_BITS);
        ledcAttachPin(pinConfig.BUZZER_PIN, LEDC_CHANNEL);
        ledcWrite(LEDC_CHANNEL, 0);
        
        initialized = true;
        log_i("BuzzerController setup on pin %d, channel %d", pinConfig.BUZZER_PIN, LEDC_CHANNEL);

        // Short power-on chirp so the user knows the buzzer works.
        beep(1, 3000, 60, 0);
        log_i("BuzzerController: Power-on chirp played");
    }

    void tone(uint32_t freq, uint32_t durationMs)
    {
        if (!initialized) return;
        seqRemaining = 0; // cancel any sequence
        hwTone(freq);
        toneStopAtMs = (durationMs > 0) ? millis() + durationMs : 0;
    }

    void stop()
    {
        seqRemaining = 0;
        toneStopAtMs = 0;
        hwOff();
    }

    void beep(uint8_t count, uint32_t freq, uint32_t durationMs, uint32_t gapMs)
    {
        if (!initialized || count == 0) {
            log_i("beep: invalid parameters or buzzer not initialized (count=%d freq=%d duration=%d gap=%d)", count, freq, durationMs, gapMs);
            return;
        }
        seqRemaining = count;
        seqFreq = freq;
        seqOnMs = durationMs;
        seqOffMs = gapMs;
        seqInOnPhase = true;
        seqNextAtMs = millis() + seqOnMs;
        toneStopAtMs = 0;
        hwTone(seqFreq);
    }

    bool preset(const char *name)
    {
        if (!name) return false;
        if (!strcasecmp(name, "ok") || !strcasecmp(name, "success"))
        {
            beep(1, 4000, 120, 0);
            return true;
        }
        if (!strcasecmp(name, "click"))
        {
            beep(1, 6000, 20, 0);
            return true;
        }
        if (!strcasecmp(name, "warn") || !strcasecmp(name, "warning"))
        {
            beep(2, 2500, 100, 100);
            return true;
        }
        if (!strcasecmp(name, "error") || !strcasecmp(name, "fail"))
        {
            beep(3, 1500, 150, 100);
            return true;
        }
        if (!strcasecmp(name, "shutdown"))
        {
            beep(1, 800, 400, 0);
            return true;
        }
        return false;
    }

    void loop()
    {
        if (!initialized) return;
        uint32_t now = millis();

        // Single-shot tone with explicit duration
        if (toneStopAtMs && (int32_t)(now - toneStopAtMs) >= 0)
        {
            toneStopAtMs = 0;
            hwOff();
        }

        // Beep sequence state machine
        if (seqRemaining > 0 && (int32_t)(now - seqNextAtMs) >= 0)
        {
            if (seqInOnPhase)
            {
                // ON just finished -> start gap
                hwOff();
                if (--seqRemaining == 0)
                {
                    return; // sequence done
                }
                seqInOnPhase = false;
                seqNextAtMs = now + seqOffMs;
            }
            else
            {
                // OFF just finished -> next ON
                hwTone(seqFreq);
                seqInOnPhase = true;
                seqNextAtMs = now + seqOnMs;
            }
        }
    }

    int act(cJSON *root)
    {
        cJSON *jqid = cJSON_GetObjectItem(root, "qid");
        int qid = (jqid && cJSON_IsNumber(jqid)) ? jqid->valueint : 0;
        log_i("/buzzer_act: qid=%d", qid);
        cJSON *bz = cJSON_GetObjectItem(root, "buzzer");
        if (!bz || !cJSON_IsObject(bz))
        {
            log_w("/buzzer_act: missing 'buzzer' object");
            return qid;
        }

        cJSON *jstop = cJSON_GetObjectItem(bz, "stop");
        if (jstop && (cJSON_IsTrue(jstop) || (cJSON_IsNumber(jstop) && jstop->valueint != 0)))
        {
            stop();
            return qid;
        }

        cJSON *jpreset = cJSON_GetObjectItem(bz, "preset");
        if (jpreset && cJSON_IsString(jpreset))
        {
            if (!preset(jpreset->valuestring))
                log_w("/buzzer_act: unknown preset '%s'", jpreset->valuestring);
            return qid;
        }

        cJSON *jfreq = cJSON_GetObjectItem(bz, "freq");
        cJSON *jdur  = cJSON_GetObjectItem(bz, "duration");
        cJSON *jbeeps = cJSON_GetObjectItem(bz, "beeps");
        cJSON *jgap  = cJSON_GetObjectItem(bz, "gap");

        uint32_t freq = (jfreq && cJSON_IsNumber(jfreq)) ? (uint32_t)jfreq->valueint : 2000;
        uint32_t dur  = (jdur  && cJSON_IsNumber(jdur))  ? (uint32_t)jdur->valueint  : 100;
        uint32_t gap  = (jgap  && cJSON_IsNumber(jgap))  ? (uint32_t)jgap->valueint  : 80;
        uint8_t  cnt  = (jbeeps && cJSON_IsNumber(jbeeps)) ? (uint8_t)jbeeps->valueint : 1;
        if (cnt == 0) cnt = 1;

        if (cnt > 1)
            beep(cnt, freq, dur, gap);
        else
            tone(freq, dur);

        return qid;
    }
}
