// PtzKeyboard.cpp — UART1 RS-485 receiver task + Pelco frame → Motion/Event
// fan-out. See PtzKeyboard.h for the wiring / debug workflow.
#include "PtzKeyboard.h"
#include "PinConfig.h"

#ifdef PTZ_KEYBOARD_CONTROLLER

#include <Arduino.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/task.h"
#include "PelcoParser.h"
#include "../serial/SerialProcess.h"

#ifdef CAN_CONTROLLER_CANOPEN
extern "C" {
#include <CANopen.h>
#include "OD.h"
}
#endif

static const char* TAG = "ptz_kbd";

namespace {

constexpr uart_port_t kUart       = UART_NUM_1;
constexpr int         kRxBufBytes = 512;

// Shared state — written by the RX task, read by PtzRouter / /ptz_get.
portMUX_TYPE            s_lock = portMUX_INITIALIZER_UNLOCKED;
PtzKeyboard::Motion     s_motion;
bool                    s_haveMotion = false;
QueueHandle_t           s_events     = nullptr;

Pelco::Parser           s_parser;
Pelco::Frame            s_lastFrame;          // RX-task only, snapshot under lock
uint32_t                s_framesDropped = 0;  // wrong address

// Runtime-tunable via /ptz_act (defaults from PinConfig)
volatile uint8_t  s_debug     = 0;  // 0 quiet, 1 decoded frames, 2 + raw hex
volatile uint8_t  s_addrFilter = 0; // 0 = accept any camera address
volatile uint32_t s_timeoutMs  = 0; // motion watchdog, 0 = off

// Edge state for discrete keys that ride in motion frames.
int8_t s_prevIris = 0;
bool   s_prevAutoScan = false, s_prevCamOnOff = false;

void pushEvent(uint8_t type, uint8_t arg)
{
    PtzKeyboard::Event ev{type, arg};
    if (s_events) xQueueSend(s_events, &ev, 0); // drop on overflow, never block
    log_i("PTZ event type=%u arg=%u", type, arg);
}

void debugHex(const char* tag, const uint8_t* d, size_t n)
{
    // 3 chars per byte + tag; cap chunks so the line stays readable.
    char line[3 * 32 + 32];
    size_t off = (size_t)snprintf(line, sizeof(line), "{\"ptz_%s\":\"", tag);
    for (size_t i = 0; i < n && i < 32; ++i)
        off += (size_t)snprintf(line + off, sizeof(line) - off, "%02X ", d[i]);
    if (off > 0 && line[off - 1] == ' ') off--;
    snprintf(line + off, sizeof(line) - off, "\"}");
    SerialProcess::safePrintln(line);
}

void debugFrame(const Pelco::Frame& f, const Pelco::Action& a)
{
    cJSON* root = cJSON_CreateObject();
    if (!root) return;
    cJSON* p = cJSON_AddObjectToObject(root, "ptz_frame");
    if (p) {
        cJSON_AddStringToObject(p, "proto", f.proto == Pelco::Proto::D ? "D" : "P");
        cJSON_AddNumberToObject(p, "addr",  f.addr);
        char hex[3 * 8 + 1] = {0};
        for (int i = 0; i < f.rawLen; ++i)
            snprintf(hex + 3 * i, 4, "%02X ", f.raw[i]);
        hex[3 * f.rawLen - 1] = 0;
        cJSON_AddStringToObject(p, "raw", hex);
        if (a.ext != Pelco::Ext::None) {
            cJSON_AddNumberToObject(p, "ext",    (int)a.ext);
            cJSON_AddNumberToObject(p, "extArg", a.extArg);
        } else if (a.stop) {
            cJSON_AddNumberToObject(p, "stop", 1);
        } else {
            cJSON_AddNumberToObject(p, "pan",   a.panDir  * (int)a.panSpeed);
            cJSON_AddNumberToObject(p, "tilt",  a.tiltDir * (int)a.tiltSpeed);
            cJSON_AddNumberToObject(p, "zoom",  a.zoomDir);
            cJSON_AddNumberToObject(p, "focus", a.focusDir);
            cJSON_AddNumberToObject(p, "iris",  a.irisDir);
        }
    }
    SerialProcess::safeSerializeJson(root); // serializes only, we keep ownership
    cJSON_Delete(root);
}

void handleFrame(const Pelco::Frame& f)
{
    if (s_addrFilter != 0 && f.addr != s_addrFilter) {
        s_framesDropped++;
        log_w("PTZ frame addr %u != filter %u → dropped (%u total)",
              f.addr, s_addrFilter, (unsigned)s_framesDropped);
        return;
    }

    const Pelco::Action a = Pelco::decode(f);
    if (s_debug >= 1) debugFrame(f, a);

    if (a.ext != Pelco::Ext::None) {
        switch (a.ext) {
            case Pelco::Ext::PresetCall:  pushEvent(PtzKeyboard::EV_PRESET_CALL,  a.extArg); break;
            case Pelco::Ext::PresetSet:   pushEvent(PtzKeyboard::EV_PRESET_SET,   a.extArg); break;
            case Pelco::Ext::PresetClear: pushEvent(PtzKeyboard::EV_PRESET_CLEAR, a.extArg); break;
            case Pelco::Ext::AuxOn:       pushEvent(PtzKeyboard::EV_AUX_ON,       a.extArg); break;
            case Pelco::Ext::AuxOff:      pushEvent(PtzKeyboard::EV_AUX_OFF,      a.extArg); break;
            default: break; // Unknown extended opcodes visible via debug:1
        }
        return;
    }

    // Discrete keys that arrive as motion-frame bits → rising-edge events.
    if (a.irisDir == +1 && s_prevIris != +1) pushEvent(PtzKeyboard::EV_IRIS_OPEN,  0);
    if (a.irisDir == -1 && s_prevIris != -1) pushEvent(PtzKeyboard::EV_IRIS_CLOSE, 0);
    s_prevIris = a.irisDir;
    if (a.autoScan && !s_prevAutoScan)       pushEvent(PtzKeyboard::EV_AUTOSCAN,     0);
    s_prevAutoScan = a.autoScan;
    if (a.cameraOnOff && !s_prevCamOnOff)    pushEvent(PtzKeyboard::EV_CAMERA_ONOFF, 0);
    s_prevCamOnOff = a.cameraOnOff;

    PtzKeyboard::Motion m;
    m.panDir    = a.panDir;
    m.tiltDir   = a.tiltDir;
    m.panSpeed  = a.panSpeed;
    m.tiltSpeed = a.tiltSpeed;
    m.zoomDir   = a.zoomDir;
    m.focusDir  = a.focusDir;
    m.irisDir   = a.irisDir;
    m.updatedMs = millis();
    // a.stop leaves all fields zero — exactly the "stop everything" snapshot.

    portENTER_CRITICAL(&s_lock);
    s_motion     = m;
    s_lastFrame  = f;
    s_haveMotion = true;
    portEXIT_CRITICAL(&s_lock);
}

void rx_task(void* /*arg*/)
{
    uint8_t chunk[64];
    for (;;) {
        int n = uart_read_bytes(kUart, chunk, sizeof(chunk), pdMS_TO_TICKS(20));
        if (n <= 0) continue;
        if (s_debug >= 2) debugHex("rx", chunk, (size_t)n);
        const uint32_t now = millis();
        Pelco::Frame f;
        for (int i = 0; i < n; ++i)
            if (s_parser.feed(chunk[i], now, f))
                handleFrame(f);
    }
}

} // namespace

namespace PtzKeyboard {

void setup()
{
#ifdef CAN_CONTROLLER_CANOPEN
    // Enable TPDO2 (clear bit 31, keep bit 30 so the stack adds the node-id)
    // for the upstream key-event frames PtzRouter emits. Must happen BEFORE
    // canopenModule.setup() builds its TPDO descriptors — same contract as
    // GpioCanSlave::enableTpdo2() (see main.cpp ordering).
    OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.COB_IDUsedByTPDO = 0x40000280u;
#endif

    if (pinConfig.PTZ_RS485_RX < 0) {
        log_e("PTZ_RS485_RX not configured — PTZ keyboard disabled");
        return;
    }
    s_debug     = pinConfig.PTZ_DEBUG_DEFAULT;
    s_addrFilter = pinConfig.PTZ_CAMERA_ADDRESS;
    s_timeoutMs  = pinConfig.PTZ_MOTION_TIMEOUT_MS;

    uart_config_t cfg = {};
    cfg.baud_rate  = (int)pinConfig.PTZ_BAUDRATE;
    cfg.data_bits  = UART_DATA_8_BITS;
    cfg.parity     = UART_PARITY_DISABLE;
    cfg.stop_bits  = UART_STOP_BITS_1;
    cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_APB;
    ESP_ERROR_CHECK(uart_driver_install(kUart, kRxBufBytes, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(kUart, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(kUart,
                                 pinConfig.PTZ_RS485_TX >= 0 ? pinConfig.PTZ_RS485_TX : UART_PIN_NO_CHANGE,
                                 pinConfig.PTZ_RS485_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    s_events = xQueueCreate(16, sizeof(Event));
    xTaskCreatePinnedToCore(rx_task, "ptz_rx", 4096, NULL, 4, NULL, 0);
    log_i("PTZ keyboard on UART1 RX=%d TX=%d @ %ld baud (addr filter %u, debug %u)",
          pinConfig.PTZ_RS485_RX, pinConfig.PTZ_RS485_TX,
          (long)pinConfig.PTZ_BAUDRATE, (unsigned)s_addrFilter, (unsigned)s_debug);
}

bool getMotion(Motion& out)
{
    portENTER_CRITICAL(&s_lock);
    out = s_motion;
    const bool have = s_haveMotion;
    portEXIT_CRITICAL(&s_lock);
    return have;
}

QueueHandle_t getEventQueue() { return s_events; }

uint32_t motionTimeoutMs() { return s_timeoutMs; }

cJSON* act(cJSON* doc)
{
    cJSON* v;
    if ((v = cJSON_GetObjectItemCaseSensitive(doc, "debug")) && cJSON_IsNumber(v))
        s_debug = (uint8_t)v->valueint;
    if ((v = cJSON_GetObjectItemCaseSensitive(doc, "addr")) && cJSON_IsNumber(v))
        s_addrFilter = (uint8_t)v->valueint;
    if ((v = cJSON_GetObjectItemCaseSensitive(doc, "timeout")) && cJSON_IsNumber(v))
        s_timeoutMs = (uint32_t)v->valueint;

    cJSON* ret = cJSON_CreateObject();
    if (!ret) return nullptr;
    cJSON* p = cJSON_AddObjectToObject(ret, "ptz");
    if (p) {
        cJSON_AddNumberToObject(p, "debug",   s_debug);
        cJSON_AddNumberToObject(p, "addr",    s_addrFilter);
        cJSON_AddNumberToObject(p, "timeout", s_timeoutMs);
    }
    return ret;
}

cJSON* get(cJSON* /*doc*/)
{
    Motion m;
    Pelco::Frame f;
    portENTER_CRITICAL(&s_lock);
    m = s_motion;
    f = s_lastFrame;
    portEXIT_CRITICAL(&s_lock);

    cJSON* ret = cJSON_CreateObject();
    if (!ret) return nullptr;
    cJSON* p = cJSON_AddObjectToObject(ret, "ptz");
    if (!p) return ret;
    cJSON_AddNumberToObject(p, "bytesIn",   s_parser.bytesIn);
    cJSON_AddNumberToObject(p, "framesOk",  s_parser.framesOk);
    cJSON_AddNumberToObject(p, "resyncs",   s_parser.resyncs);
    cJSON_AddNumberToObject(p, "gapResets", s_parser.gapResets);
    cJSON_AddNumberToObject(p, "dropped",   s_framesDropped);
    cJSON_AddNumberToObject(p, "debug",     s_debug);
    cJSON_AddNumberToObject(p, "addr",      s_addrFilter);
    cJSON_AddNumberToObject(p, "timeout",   s_timeoutMs);
    if (f.rawLen > 0) {
        char hex[3 * 8 + 1] = {0};
        for (int i = 0; i < f.rawLen; ++i)
            snprintf(hex + 3 * i, 4, "%02X ", f.raw[i]);
        hex[3 * f.rawLen - 1] = 0;
        cJSON_AddStringToObject(p, "lastFrame", hex);
        cJSON_AddNumberToObject(p, "lastAddr",  f.addr);
    }
    cJSON* mo = cJSON_AddObjectToObject(p, "motion");
    if (mo) {
        cJSON_AddNumberToObject(mo, "pan",   m.panDir  * (int)m.panSpeed);
        cJSON_AddNumberToObject(mo, "tilt",  m.tiltDir * (int)m.tiltSpeed);
        cJSON_AddNumberToObject(mo, "zoom",  m.zoomDir);
        cJSON_AddNumberToObject(mo, "focus", m.focusDir);
        cJSON_AddNumberToObject(mo, "ageMs", m.updatedMs ? (int)(millis() - m.updatedMs) : -1);
    }
    return ret;
}

} // namespace PtzKeyboard

#endif // PTZ_KEYBOARD_CONTROLLER
