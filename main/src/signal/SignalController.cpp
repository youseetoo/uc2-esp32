#include "SignalController.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace SignalController
{
    static const char *TAG = "SignalController";

    static Adafruit_NeoPixel *strip = nullptr;
    static uint16_t numPixels = 20;

    static SignalStatus currentStatus = SignalStatus::UNKNOWN;
    static SignalPattern currentPattern;
    static uint8_t globalBrightness = 128;

    // Animation state
    static uint8_t segIndex = 0;        // index into stops[] of "from" color
    static uint32_t segStartMs = 0;
    static bool inFade = false;          // true: fading from segIndex to next; false: lingering on segIndex

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------
    static inline uint8_t clamp8(int v)
    {
        if (v < 0) return 0;
        if (v > 255) return 255;
        return (uint8_t)v;
    }

    // Parse "#RRGGBB" or "RRGGBB"; returns true on success.
    static bool parseHexColor(const char *s, uint8_t &r, uint8_t &g, uint8_t &b)
    {
        if (!s) return false;
        if (*s == '#') s++;
        if (strlen(s) < 6) return false;
        char buf[3] = {0};
        buf[0] = s[0]; buf[1] = s[1]; r = (uint8_t)strtoul(buf, nullptr, 16);
        buf[0] = s[2]; buf[1] = s[3]; g = (uint8_t)strtoul(buf, nullptr, 16);
        buf[0] = s[4]; buf[1] = s[5]; b = (uint8_t)strtoul(buf, nullptr, 16);
        return true;
    }

    // The "skip" sentinel from the brand guide table.
    static bool isSkipColor(uint8_t r, uint8_t g, uint8_t b)
    {
        return r == 0xC0 && g == 0xFF && b == 0xEE;
    }

    static ColorStop makeStop(const char *hex, uint16_t lingerSeconds)
    {
        ColorStop s;
        uint8_t r = 0, g = 0, b = 0;
        if (parseHexColor(hex, r, g, b))
        {
            s.r = r; s.g = g; s.b = b;
        }
        s.lingerMs = (uint16_t)(lingerSeconds * 1000);
        s.active = !(isSkipColor(s.r, s.g, s.b) && lingerSeconds == 0);
        return s;
    }

    // -----------------------------------------------------------------------
    // Predefined patterns from the FRAME brand guide
    // -----------------------------------------------------------------------
    static SignalPattern makePattern(const char *p1, uint16_t d1,
                                     const char *p2, uint16_t d2,
                                     const char *p3, uint16_t d3)
    {
        SignalPattern p;
        p.stops[0] = makeStop(p1, d1);
        p.stops[1] = makeStop(p2, d2);
        p.stops[2] = makeStop(p3, d3);
        p.fadeMs = 500;
        return p;
    }

    static SignalPattern patternFor(SignalStatus s)
    {
        switch (s)
        {
        case SignalStatus::INITIALIZING:
            return makePattern("#FFFFFF", 1, "#000000", 1, "#c0ffee", 0);
        case SignalStatus::INIT_FAILED:
            return makePattern("#730502", 3, "#000000", 3, "#000000", 3);
        case SignalStatus::IDLE:
            return makePattern("#85b918", 3, "#85b918", 3, "#000000", 3);
        case SignalStatus::CLIENT_ACTIVE:
            return makePattern("#8F18B9", 1, "#8F18B9", 1, "#B91878", 1);
        case SignalStatus::JOB_RUNNING:
            return makePattern("#735C02", 1, "#734202", 1, "#c0ffee", 0);
        case SignalStatus::JOB_FAILED:
            return makePattern("#730502", 1, "#000000", 1, "#c0ffee", 0);
        case SignalStatus::JOB_PARTLY_FAILED:
            return makePattern("#730502", 3, "#85b918", 3, "#c0ffee", 0);
        case SignalStatus::JOB_COMPLETE:
            return makePattern("#85b918", 3, "#85b918", 3, "#734202", 3);
        case SignalStatus::RASPI_BUSY:
            return makePattern("#85b918", 1, "#85b918", 1, "#8F18B9", 1);
        case SignalStatus::RASPI_SHUTDOWN:
            return makePattern("#85b918", 1, "#FFFFFF", 1, "#000000", 1);
        case SignalStatus::ON:
            return makePattern("#FFFFFF", 1, "#FFFFFF", 1, "#c0ffee", 0);
        case SignalStatus::OFF:
        default:
            return makePattern("#000000", 1, "#000000", 1, "#c0ffee", 0);
        }
    }

    // Map "name" -> SignalStatus.
    static SignalStatus statusFromName(const char *name)
    {
        if (!name) return SignalStatus::UNKNOWN;
        if (!strcasecmp(name, "off"))                  return SignalStatus::OFF;
        if (!strcasecmp(name, "on"))                   return SignalStatus::ON;
        if (!strcasecmp(name, "init") ||
            !strcasecmp(name, "initializing"))         return SignalStatus::INITIALIZING;
        if (!strcasecmp(name, "init_failed") ||
            !strcasecmp(name, "init-failed") ||
            !strcasecmp(name, "fatal"))                return SignalStatus::INIT_FAILED;
        if (!strcasecmp(name, "idle"))                 return SignalStatus::IDLE;
        if (!strcasecmp(name, "client") ||
            !strcasecmp(name, "client_active") ||
            !strcasecmp(name, "active"))               return SignalStatus::CLIENT_ACTIVE;
        if (!strcasecmp(name, "busy") ||
            !strcasecmp(name, "job") ||
            !strcasecmp(name, "running") ||
            !strcasecmp(name, "job_running"))          return SignalStatus::JOB_RUNNING;
        if (!strcasecmp(name, "error") ||
            !strcasecmp(name, "failed") ||
            !strcasecmp(name, "job_failed"))           return SignalStatus::JOB_FAILED;
        if (!strcasecmp(name, "warn") ||
            !strcasecmp(name, "warning") ||
            !strcasecmp(name, "job_partly_failed") ||
            !strcasecmp(name, "partly"))               return SignalStatus::JOB_PARTLY_FAILED;
        if (!strcasecmp(name, "done") ||
            !strcasecmp(name, "complete") ||
            !strcasecmp(name, "job_complete") ||
            !strcasecmp(name, "success"))              return SignalStatus::JOB_COMPLETE;
        if (!strcasecmp(name, "raspi_busy") ||
            !strcasecmp(name, "transfer"))             return SignalStatus::RASPI_BUSY;
        if (!strcasecmp(name, "raspi_shutdown") ||
            !strcasecmp(name, "shutdown"))             return SignalStatus::RASPI_SHUTDOWN;
        return SignalStatus::UNKNOWN;
    }

    static const char *nameFromStatus(SignalStatus s)
    {
        switch (s)
        {
        case SignalStatus::OFF:                return "off";
        case SignalStatus::INITIALIZING:       return "initializing";
        case SignalStatus::INIT_FAILED:        return "init_failed";
        case SignalStatus::IDLE:               return "idle";
        case SignalStatus::CLIENT_ACTIVE:      return "client_active";
        case SignalStatus::JOB_RUNNING:        return "job_running";
        case SignalStatus::JOB_FAILED:         return "job_failed";
        case SignalStatus::JOB_PARTLY_FAILED:  return "job_partly_failed";
        case SignalStatus::JOB_COMPLETE:       return "job_complete";
        case SignalStatus::RASPI_BUSY:         return "raspi_busy";
        case SignalStatus::RASPI_SHUTDOWN:     return "raspi_shutdown";
        case SignalStatus::ON:                 return "on";
        case SignalStatus::CUSTOM:             return "custom";
        default:                               return "unknown";
        }
    }

    // Find next active stop after `from`. Returns from if none other is active.
    static uint8_t nextActiveStop(uint8_t from)
    {
        for (uint8_t i = 1; i <= 3; i++)
        {
            uint8_t idx = (from + i) % 3;
            if (currentPattern.stops[idx].active) return idx;
        }
        return from;
    }

    static uint8_t firstActiveStop()
    {
        for (uint8_t i = 0; i < 3; i++)
            if (currentPattern.stops[i].active) return i;
        return 0;
    }

    // -----------------------------------------------------------------------
    // Public API
    // -----------------------------------------------------------------------
    void setup()
    {
        if (pinConfig.LED_PIN < 0)
        {
            log_w("SignalController: LED_PIN disabled, skipping setup");
            return;
        }
        if (!pinConfig.IS_STATUS_LED)
        {
            log_i("SignalController: IS_STATUS_LED=false, signal disabled");
            return;
        }

        // Number of pixels for the status indicator. The FRAME HAT v2 has 1
        // on-board pixel; allow a small strip via LED_COUNT.
        numPixels = (pinConfig.LED_COUNT > 0) ? (uint16_t)pinConfig.LED_COUNT : 1;
        // TODO: Check if we already used it in ledarr
        strip = new Adafruit_NeoPixel(numPixels, pinConfig.LED_PIN, NEO_GRB + NEO_KHZ800);
        strip->begin();
        strip->setBrightness(globalBrightness);
        strip->clear();
        strip->show();

        setStatus(SignalStatus::INITIALIZING);
        log_i("SignalController setup on pin %d, %d pixel(s)", pinConfig.LED_PIN, numPixels);
    }

    void setBrightness(uint8_t b)
    {
        globalBrightness = b;
        if (strip) strip->setBrightness(b);
    }

    void setStatus(SignalStatus s)
    {
        currentStatus = s;
        currentPattern = patternFor(s);
        segIndex = firstActiveStop();
        segStartMs = millis();
        inFade = false;
        log_i("SignalController status -> %s", nameFromStatus(s));
    }

    bool setStatusByName(const char *name)
    {
        SignalStatus s = statusFromName(name);
        log_i("SignalController setStatusByName('%s') -> %s", name, nameFromStatus(s));
        if (s == SignalStatus::UNKNOWN) return false;
        setStatus(s);
        return true;
    }

    void setCustom(const SignalPattern &p)
    {
        currentPattern = p;
        currentStatus = SignalStatus::CUSTOM;
        segIndex = firstActiveStop();
        segStartMs = millis();
        inFade = false;
    }

    // Apply gamma-corrected color to all pixels.
    static void writeColor(uint8_t r, uint8_t g, uint8_t b)
    {
        if (!strip) return;
        uint32_t c = strip->Color(strip->gamma8(r), strip->gamma8(g), strip->gamma8(b));
        for (uint16_t i = 0; i < numPixels; i++) strip->setPixelColor(i, c);
        strip->show();
    }

    void loop()
    {
        if (!strip) return;
        if (currentStatus == SignalStatus::UNKNOWN) return;

        uint32_t now = millis();
        ColorStop &cur = currentPattern.stops[segIndex];

        if (!inFade)
        {
            // Holding the current color
            writeColor(cur.r, cur.g, cur.b);
            if (now - segStartMs >= cur.lingerMs)
            {
                inFade = true;
                segStartMs = now;
            }
            return;
        }

        // Fading from cur -> next
        uint8_t nextIdx = nextActiveStop(segIndex);
        ColorStop &nxt = currentPattern.stops[nextIdx];
        uint16_t fadeMs = currentPattern.fadeMs ? currentPattern.fadeMs : 1;
        uint32_t dt = now - segStartMs;
        if (dt >= fadeMs || nextIdx == segIndex)
        {
            segIndex = nextIdx;
            inFade = false;
            segStartMs = now;
            writeColor(nxt.r, nxt.g, nxt.b);
            return;
        }
        float t = (float)dt / (float)fadeMs;
        uint8_t r = clamp8((int)(cur.r + (int)(((int)nxt.r - (int)cur.r) * t)));
        uint8_t g = clamp8((int)(cur.g + (int)(((int)nxt.g - (int)cur.g) * t)));
        uint8_t b = clamp8((int)(cur.b + (int)(((int)nxt.b - (int)cur.b) * t)));
        writeColor(r, g, b);
    }

    // -----------------------------------------------------------------------
    // JSON helpers
    // -----------------------------------------------------------------------
    static bool parseStop(cJSON *obj, ColorStop &out)
    {
        if (!obj || !cJSON_IsObject(obj)) return false;
        cJSON *jc = cJSON_GetObjectItem(obj, "color");
        cJSON *jd = cJSON_GetObjectItem(obj, "duration");
        uint16_t durationS = 1;
        if (jd && cJSON_IsNumber(jd)) durationS = (uint16_t)jd->valueint;
        const char *hex = (jc && cJSON_IsString(jc)) ? jc->valuestring : "#000000";
        out = makeStop(hex, durationS);
        return true;
    }

    int act(cJSON *root)
    {
        cJSON *jqid = cJSON_GetObjectItem(root, "qid");
        int qid = (jqid && cJSON_IsNumber(jqid)) ? jqid->valueint : 0;

        cJSON *sig = cJSON_GetObjectItem(root, "signal");
        if (!sig || !cJSON_IsObject(sig))
        {
            log_w("/signal_act: missing 'signal' object");
            return qid;
        }

        // Optional brightness applies in all cases.
        cJSON *jbr = cJSON_GetObjectItem(sig, "brightness");
        if (jbr && cJSON_IsNumber(jbr))
            setBrightness((uint8_t)jbr->valueint);

        // Status by name OR full custom pattern.
        cJSON *jstatus = cJSON_GetObjectItem(sig, "status");
        cJSON *jprim   = cJSON_GetObjectItem(sig, "primary");
        cJSON *jsec    = cJSON_GetObjectItem(sig, "secondary");
        cJSON *jtert   = cJSON_GetObjectItem(sig, "tertiary");

        bool hasCustomColors = (jprim || jsec || jtert);
        const char *statusName = (jstatus && cJSON_IsString(jstatus)) ? jstatus->valuestring : nullptr;

        if (statusName && strcasecmp(statusName, "custom") != 0 && !hasCustomColors)
        {
            if (!setStatusByName(statusName))
                log_w("/signal_act: unknown status '%s'", statusName);
            return qid;
        }

        if (hasCustomColors || (statusName && !strcasecmp(statusName, "custom")))
        {
            SignalPattern p;
            p.fadeMs = 500;
            cJSON *jfade = cJSON_GetObjectItem(sig, "fade_ms");
            if (jfade && cJSON_IsNumber(jfade)) p.fadeMs = (uint16_t)jfade->valueint;
            if (jprim) parseStop(jprim, p.stops[0]);
            if (jsec)  parseStop(jsec,  p.stops[1]);
            else       p.stops[1].active = false;
            if (jtert) parseStop(jtert, p.stops[2]);
            else       p.stops[2].active = false;
            setCustom(p);
        }
        return qid;
    }

    cJSON *get(cJSON *root)
    {
        cJSON *j = cJSON_CreateObject();
        cJSON *s = cJSON_CreateObject();
        cJSON_AddItemToObject(j, "signal", s);
        cJSON_AddStringToObject(s, "status", nameFromStatus(currentStatus));
        cJSON_AddNumberToObject(s, "brightness", globalBrightness);
        cJSON_AddNumberToObject(s, "pixels", numPixels);
        return j;
    }
}
