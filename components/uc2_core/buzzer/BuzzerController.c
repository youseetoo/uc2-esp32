// T4.1: pure C (was the C++ namespace BuzzerController).
#include <string.h>
#include <strings.h>      // strcasecmp
#include "BuzzerController.h"

// T1.3: direct IDF 6.1 (no Arduino):
#include "driver/ledc.h"          // LEDC v2 (esp_driver_ledc)
#include "uc2_hal/uc2_log.h"      // log_* -> ESP_LOG*
#include "uc2_hal/uc2_time.h"     // millis -> esp_timer

static const char *TAG = "BuzzerController";

// Arduino ch 7 == IDF LEDC v2 LEDC_CHANNEL_7 = timer 3, slot 1 (same HW).
// 8-bit duty resolution -> duty 0..255; 50% = 128.
static const ledc_timer_t   LEDC_TIMER = LEDC_TIMER_3;
static const ledc_channel_t LEDC_CHANNEL = LEDC_CHANNEL_7;
static const uint32_t       LEDC_DUTY_50 = 128;

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

static void hwOff(void)
{
    if (!initialized) return;
    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, 0, 0);
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
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER, freq);
    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, LEDC_DUTY_50, 0);
    toneActive = true;
}

void buzzer_setup(void)
{
    if (pinConfig.BUZZER_PIN < 0)
    {
        log_i("BuzzerController: BUZZER_PIN disabled");
        return;
    }
    // Direct IDF LEDC v2 (was ledcSetup(7, 2000, 8) + ledcAttachPin(pin, 7)).
    ledc_timer_config_t tc = {};
    tc.speed_mode      = LEDC_LOW_SPEED_MODE;
    tc.duty_resolution = LEDC_TIMER_8_BIT;
    tc.timer_num       = LEDC_TIMER;
    tc.freq_hz         = 2000;
    if (ledc_timer_config(&tc) != ESP_OK)
    {
        log_e("BuzzerController: ledc_timer_config failed");
        return;
    }
    ledc_channel_config_t cc = {};
    cc.gpio_num   = pinConfig.BUZZER_PIN;
    cc.speed_mode = LEDC_LOW_SPEED_MODE;
    cc.channel    = LEDC_CHANNEL;
    cc.timer_sel  = LEDC_TIMER;
    cc.duty       = 0;
    if (ledc_channel_config(&cc) != ESP_OK)
    {
        log_e("BuzzerController: ledc_channel_config failed");
        return;
    }

    initialized = true;
    log_i("BuzzerController setup on pin %d, channel %d", pinConfig.BUZZER_PIN, LEDC_CHANNEL);

    // Short power-on chirp so the user knows the buzzer works.
    buzzer_beep(1, 440, 60, 0);
    log_i("BuzzerController: Power-on chirp played");
}

void buzzer_tone(uint32_t freq, uint32_t durationMs)
{
    if (!initialized) return;
    seqRemaining = 0; // cancel any sequence
    hwTone(freq);
    toneStopAtMs = (durationMs > 0) ? millis() + durationMs : 0;
}

void buzzer_stop(void)
{
    seqRemaining = 0;
    toneStopAtMs = 0;
    hwOff();
}

void buzzer_beep(uint8_t count, uint32_t freq, uint32_t durationMs, uint32_t gapMs)
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

bool buzzer_preset(const char *name)
{
    if (!name) return false;
    if (!strcasecmp(name, "ok") || !strcasecmp(name, "success"))
    {
        buzzer_beep(1, 4000, 120, 0);
        return true;
    }
    if (!strcasecmp(name, "click"))
    {
        buzzer_beep(1, 6000, 20, 0);
        return true;
    }
    if (!strcasecmp(name, "warn") || !strcasecmp(name, "warning"))
    {
        buzzer_beep(2, 2500, 100, 100);
        return true;
    }
    if (!strcasecmp(name, "error") || !strcasecmp(name, "fail"))
    {
        buzzer_beep(3, 1500, 150, 100);
        return true;
    }
    if (!strcasecmp(name, "shutdown"))
    {
        buzzer_beep(1, 800, 400, 0);
        return true;
    }
    return false;
}

void buzzer_loop(void)
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

int buzzer_act(cJSON *root)
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
        buzzer_stop();
        return qid;
    }

    cJSON *jpreset = cJSON_GetObjectItem(bz, "preset");
    if (jpreset && cJSON_IsString(jpreset))
    {
        if (!buzzer_preset(jpreset->valuestring))
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
        buzzer_beep(cnt, freq, dur, gap);
    else
        buzzer_tone(freq, dur);

    return qid;
}
