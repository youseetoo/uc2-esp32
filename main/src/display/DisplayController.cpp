#include "DisplayController.h"
#include "DisplayGfx.h"
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <string.h>
#include "cJsonTool.h"
#include "uc2_fw_version.h"

namespace DisplayController
{
    using namespace DisplayGfx;

    static constexpr size_t   TEXT_LEN      = 64;   // hostnames are <= 63 chars
    static constexpr uint8_t  CONTRAST_DEF  = 0x8F;
    static constexpr uint32_t TICK_MS       = 20;   // content/marquee bookkeeping cadence
    static constexpr uint32_t RENDER_MIN_MS = 30;   // never push frames faster than this
    static const char        *DEFAULT_NAME  = "openuc2";

    // ---- Layout (128x32): big title / status line / info line or progress bar ----
    enum Slot { SLOT_TITLE = 0, SLOT_STATUS, SLOT_INFO, SLOT_COUNT };
    static const int  SLOT_SCALE[SLOT_COUNT]  = {2, 1, 1};
    static const int  SLOT_Y[SLOT_COUNT]      = {0, 17, 25};   // rows 0-13 / 17-23 / 25-31
    static const bool SLOT_CENTER[SLOT_COUNT] = {true, false, false};
    static constexpr int BAR_Y = 24, BAR_H = 8;

    // ---- Persisted settings (NVS namespace "display") ----
    static Preferences dPrefs;
    static bool    powerOn  = true;
    static bool    flip     = false;
    static uint8_t contrast = CONTRAST_DEF;
    static char    name[TEXT_LEN] = "";     // "" -> DEFAULT_NAME

    // ---- Volatile content ----
    static char     status[TEXT_LEN] = "";
    static int      progress   = -1;        // 0..100, -1 = no bar
    static bool     splash     = false;     // firmware-version screen is showing
    static uint32_t splashEndMs = 0;
    static bool     splashDone = false;     // the power-up splash has been shown once

    // ---- Hardware / render state ----
    static bool     present     = false;
    static uint8_t  addr        = ADDR_PRIMARY;
    static uint32_t lastProbeMs = 0;
    static uint32_t lastTickMs  = 0;
    static uint32_t lastRenderMs = 0;
    static bool     dirty       = true;
    static bool     shadowValid = false;    // `shadow` mirrors the panel's GDDRAM
    static Frame    frame;
    static uint8_t  shadow[W * PAGES];
    static char     shown[SLOT_COUNT][TEXT_LEN];
    static Marquee  marquee[SLOT_COUNT];
    static int      shownBar    = -2;

    // ===== Text helpers =====
    // Keep printable ASCII only. UTF-8 sequences collapse to a single '?'
    // (continuation bytes are dropped) so "→" or "µ" do not become garbage runs.
    static void copyText(char *dst, const char *src)
    {
        size_t n = 0;
        for (; *src && n < TEXT_LEN - 1; ++src)
        {
            unsigned char c = (unsigned char)*src;
            if (c >= 0x80 && c < 0xC0) continue;
            dst[n++] = (c < 0x20 || c > 0x7E) ? ((c >= 0xC0) ? '?' : ' ') : (char)c;
        }
        dst[n] = '\0';
    }

    // ===== SSD1306 I2C primitives =====
    // Control byte 0x00 = the rest of the transfer is commands, 0x40 = display data.
    static bool sendCmds(const uint8_t *c, size_t n)
    {
        Wire.beginTransmission(addr);
        Wire.write((uint8_t)0x00);
        Wire.write(c, n);
        return Wire.endTransmission() == 0;
    }

    static bool sendData(const uint8_t *d, size_t n)
    {
        while (n)
        {
            size_t k = n > 64 ? 64 : n;   // Wire's buffer is 128 bytes
            Wire.beginTransmission(addr);
            Wire.write((uint8_t)0x40);
            Wire.write(d, k);
            if (Wire.endTransmission() != 0) return false;
            d += k;
            n -= k;
        }
        return true;
    }

    static bool applyPower(bool on)
    {
        // Charge pump off as well when "off": the panel then draws ~nothing.
        const uint8_t onSeq[]  = {0x8D, 0x14, 0xAF};
        const uint8_t offSeq[] = {0xAE, 0x8D, 0x10};
        return on ? sendCmds(onSeq, sizeof(onSeq)) : sendCmds(offSeq, sizeof(offSeq));
    }

    static bool applyFlip()
    {
        const uint8_t seq[] = {(uint8_t)(flip ? 0xA0 : 0xA1), (uint8_t)(flip ? 0xC0 : 0xC8)};
        return sendCmds(seq, sizeof(seq));
    }

    static bool applyContrast()
    {
        const uint8_t seq[] = {0x81, contrast};
        return sendCmds(seq, sizeof(seq));
    }

    // Push the frame; only pages that differ from what the panel already shows
    // are sent. A bus error marks the display absent so loop() re-probes it.
    static bool flush()
    {
        Wire.setClock(I2C_FLUSH_HZ);
        bool ok = true;
        for (int p = 0; p < PAGES && ok; ++p)
        {
            const uint8_t *src = &frame.buf[p * W];
            uint8_t *dst = &shadow[p * W];
            if (shadowValid && memcmp(src, dst, W) == 0) continue;
            const uint8_t window[] = {0x22, (uint8_t)p, (uint8_t)p, 0x21, 0x00, (uint8_t)(W - 1)};
            ok = sendCmds(window, sizeof(window)) && sendData(src, W);
            if (ok) memcpy(dst, src, W);
        }
        Wire.setClock(I2C_IDLE_HZ);
        shadowValid = ok;
        if (!ok)
        {
            log_w("DisplayController: I2C write failed — display marked absent");
            present = false;
            lastProbeMs = millis();
        }
        return ok;
    }

    static bool initPanel()
    {
        const uint8_t seq[] = {
            0xAE,             // display off while configuring
            0xD5, 0x80,       // oscillator / clock divide
            0xA8, H - 1,      // multiplex ratio: 32 rows
            0xD3, 0x00,       // display offset
            0x40,             // start line 0
            0x8D, 0x14,       // charge pump on
            0x20, 0x00,       // horizontal addressing mode
            0xDA, 0x02,       // COM pins: sequential (128x32 panels)
            0xD9, 0xF1,       // pre-charge period
            0xDB, 0x40,       // VCOMH deselect level
            0xA4,             // output follows RAM
            0xA6,             // normal polarity
        };
        if (!sendCmds(seq, sizeof(seq))) return false;
        if (!applyFlip() || !applyContrast()) return false;

        // Blank GDDRAM first so power-up garbage never shows, then switch on.
        frame.clear();
        shadowValid = false;
        present = true;               // flush() clears it again on a bus error
        if (!flush()) return false;
        return applyPower(powerOn);
    }

    static bool probe(uint8_t a)
    {
        Wire.beginTransmission(a);
        return Wire.endTransmission() == 0;
    }

    static bool connect()
    {
        if (probe(ADDR_PRIMARY))        addr = ADDR_PRIMARY;
        else if (probe(ADDR_SECONDARY)) addr = ADDR_SECONDARY;
        else                            return false;

        if (!initPanel())
        {
            present = false;
            return false;
        }
        if (!splashDone)
        {
            splashDone = true;
            splash = true;
            splashEndMs = millis() + SPLASH_MS;
        }
        shownBar = -2;                 // force a full content refresh
        for (int s = 0; s < SLOT_COUNT; ++s) shown[s][0] = '\x01';
        dirty = true;
        log_i("DisplayController: SSD1306 128x32 found at 0x%02X", addr);
        return true;
    }

    // ===== Content =====
    static void wantedText(int slot, char *out)
    {
        const char *t = "";
        char info[TEXT_LEN];
        switch (slot)
        {
        case SLOT_TITLE:
            t = splash ? "openUC2" : (name[0] ? name : DEFAULT_NAME);
            break;
        case SLOT_STATUS:
            t = splash ? "Firmware" : status;
            break;
        case SLOT_INFO:
            if (splash) t = UC2_FW_VERSION;
            else if (progress < 0) { snprintf(info, sizeof(info), "fw %s", UC2_FW_VERSION); t = info; }
            break;                       // progress bar replaces the info line
        }
        copyText(out, t);
    }

    // Re-evaluate what every line should show; restart a line's marquee when it changed.
    static void refreshSlots(uint32_t now)
    {
        char want[TEXT_LEN];
        for (int s = 0; s < SLOT_COUNT; ++s)
        {
            wantedText(s, want);
            if (strcmp(want, shown[s]) != 0)
            {
                strcpy(shown[s], want);
                marquee[s].reset(textWidth(shown[s], SLOT_SCALE[s]), now);
                dirty = true;
            }
        }
        int bar = (!splash && progress >= 0) ? progress : -1;
        if (bar != shownBar)
        {
            shownBar = bar;
            dirty = true;
        }
    }

    static void drawSlot(int s)
    {
        const Marquee &m = marquee[s];
        if (!m.scrolls())
        {
            int x = SLOT_CENTER[s] ? (W - m.width) / 2 : 0;
            frame.drawText(x, SLOT_Y[s], shown[s], SLOT_SCALE[s]);
        }
        else
        {
            // two copies one text-width + gap apart give a seamless wrap-around
            frame.drawText(-m.offset, SLOT_Y[s], shown[s], SLOT_SCALE[s]);
            frame.drawText(-m.offset + m.width + Marquee::GAP_PX, SLOT_Y[s], shown[s], SLOT_SCALE[s]);
        }
    }

    static void render()
    {
        frame.clear();
        for (int s = 0; s < SLOT_COUNT; ++s) drawSlot(s);
        if (shownBar >= 0)
        {
            frame.drawRect(0, BAR_Y, W, BAR_H);
            frame.fillRect(2, BAR_Y + 2, (W - 4) * shownBar / 100, BAR_H - 4);
        }
        flush();
    }

    // ===== Persistence (only written when a value actually changes: NVS wear) =====
    static void saveU8(const char *key, uint8_t v)
    {
        dPrefs.begin("display", false);
        dPrefs.putUChar(key, v);
        dPrefs.end();
    }

    static void saveName()
    {
        dPrefs.begin("display", false);
        if (name[0]) dPrefs.putString("name", name);
        else         dPrefs.remove("name");
        dPrefs.end();
    }

    // ===== Setters (used by act()) =====
    static void setPower(bool on)
    {
        if (on == powerOn) return;
        powerOn = on;
        saveU8("on", on);
        if (present && !applyPower(on)) { present = false; lastProbeMs = millis(); }
        if (on) { shadowValid = false; dirty = true; }
    }

    static void setFlip(bool f)
    {
        if (f == flip) return;
        flip = f;
        saveU8("flip", f);
        if (present && !applyFlip()) { present = false; lastProbeMs = millis(); }
    }

    static void setContrast(int c)
    {
        if (c < 0) c = 0;
        if (c > 255) c = 255;
        if (c == contrast) return;
        contrast = (uint8_t)c;
        saveU8("ctr", contrast);
        if (present && !applyContrast()) { present = false; lastProbeMs = millis(); }
    }

    static void setName(const char *n)
    {
        char tmp[TEXT_LEN];
        copyText(tmp, n);
        if (strcmp(tmp, name) == 0) return;
        strcpy(name, tmp);
        saveName();
    }

    static void setStatus(const char *s)
    {
        copyText(status, s);
    }

    // ===== Public API =====
    void setup()
    {
        if (pinConfig.I2C_SDA < 0 || pinConfig.I2C_SCL < 0)
        {
            log_w("DisplayController: I2C pins disabled — display off");
            return;
        }
        // Normally the TMP102 setup already started the bus; Wire.begin() is a
        // harmless no-op then, but keeps the display working if that module is off.
        Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, I2C_IDLE_HZ);

        dPrefs.begin("display", false);
        powerOn  = dPrefs.getBool("on", true);
        flip     = dPrefs.getBool("flip", false);
        contrast = dPrefs.getUChar("ctr", CONTRAST_DEF);
        dPrefs.getString("name", name, sizeof(name));
        dPrefs.end();

        lastProbeMs = millis();
        if (!connect()) log_i("DisplayController: no SSD1306 on I2C — will retry every %u s", (unsigned)(PROBE_INTERVAL_MS / 1000));
    }

    void loop()
    {
        uint32_t now = millis();

        if (!present)
        {
            if ((uint32_t)(now - lastProbeMs) >= PROBE_INTERVAL_MS)
            {
                lastProbeMs = now;
                connect();
            }
            return;
        }
        if (!powerOn) return;
        if ((uint32_t)(now - lastTickMs) < TICK_MS) return;
        lastTickMs = now;

        if (splash && (int32_t)(now - splashEndMs) >= 0)
        {
            splash = false;
            dirty = true;
        }
        refreshSlots(now);
        for (int s = 0; s < SLOT_COUNT; ++s)
            if (marquee[s].step(now)) dirty = true;

        if (dirty && (uint32_t)(now - lastRenderMs) >= RENDER_MIN_MS)
        {
            lastRenderMs = now;
            dirty = false;
            render();
        }
    }

    // ===== JSON I/O =====
    static bool jsonFlag(cJSON *j)
    {
        return cJSON_IsTrue(j) || (cJSON_IsNumber(j) && j->valueint != 0);
    }

    int act(cJSON *root)
    {
        int qid = cJsonTool::getJsonInt(root, "qid");
        cJSON *d = cJSON_GetObjectItem(root, "display");
        if (!d || !cJSON_IsObject(d))
        {
            log_w("/display_act: missing 'display' object");
            return qid;
        }

        cJSON *j;
        if ((j = cJSON_GetObjectItem(d, "on")) && (cJSON_IsBool(j) || cJSON_IsNumber(j)))
            setPower(jsonFlag(j));
        if ((j = cJSON_GetObjectItem(d, "flip")) && (cJSON_IsBool(j) || cJSON_IsNumber(j)))
            setFlip(jsonFlag(j));
        if ((j = cJSON_GetObjectItem(d, "contrast")) && cJSON_IsNumber(j))
            setContrast(j->valueint);
        if ((j = cJSON_GetObjectItem(d, "name")) && cJSON_IsString(j))
            setName(j->valuestring);
        if ((j = cJSON_GetObjectItem(d, "status")) && cJSON_IsString(j))
            setStatus(j->valuestring);
        if ((j = cJSON_GetObjectItem(d, "progress")) && cJSON_IsNumber(j))
        {
            int p = j->valueint;
            progress = p < 0 ? -1 : (p > 100 ? 100 : p);
        }
        if ((j = cJSON_GetObjectItem(d, "splash")) && jsonFlag(j))
        {
            splash = true;
            splashEndMs = millis() + SPLASH_MS;
        }

        dirty = true;
        log_i("/display_act: present=%d on=%d name='%s' status='%s' progress=%d", present, powerOn, name, status, progress);
        return qid;
    }

    cJSON *get(cJSON *root)
    {
        // The host matches replies to requests by qid: without it the reply
        // lands in some other command's response list and the getter times out.
        int qid = cJsonTool::getJsonInt(root, "qid");
        cJSON *ret = cJSON_CreateObject();
        cJSON *d = cJSON_CreateObject();
        cJSON_AddNumberToObject(d, "present", present ? 1 : 0);
        cJSON_AddNumberToObject(d, "on", powerOn ? 1 : 0);
        cJSON_AddNumberToObject(d, "addr", addr);
        cJSON_AddStringToObject(d, "name", name);
        cJSON_AddStringToObject(d, "status", status);
        cJSON_AddNumberToObject(d, "progress", progress);
        cJSON_AddStringToObject(d, "fw", UC2_FW_VERSION);
        cJSON_AddNumberToObject(d, "flip", flip ? 1 : 0);
        cJSON_AddNumberToObject(d, "contrast", contrast);
        cJSON_AddNumberToObject(d, "splash", splash ? 1 : 0);
        cJSON_AddItemToObject(ret, "display", d);
        cJSON_AddNumberToObject(ret, "qid", qid);
        return ret;
    }
}
