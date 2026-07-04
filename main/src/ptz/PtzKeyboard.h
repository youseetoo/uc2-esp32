// PtzKeyboard.h — RS-485 PTZ keyboard receiver.
//
// Reads Pelco-D / Pelco-P frames from a CCTV joystick keyboard via an
// RS485↔TTL transceiver on UART1 (pins from PinConfig: PTZ_RS485_RX/TX) and
// exposes:
//   * a thread-safe Motion snapshot (pan/tilt speeds, zoom/focus/iris
//     directions) that PtzRouter polls at 50 Hz, and
//   * a small event queue for discrete keys (preset set/call/clear,
//     AUX n on/off) that PtzRouter forwards to the CAN master.
//
// Wiring (adapter with automatic direction control, e.g. MAX13487 boards):
//   keyboard A/B (RJ45 pins 2/4 "RS485B"... check silk: A+/B-) → adapter A/B
//   adapter RXD → PTZ_RS485_RX, adapter TXD → PTZ_RS485_TX (unused, we only
//   listen), 3V3 + GND shared. Keyboard menu: 995+AUX → BAUDRATE 9600 →
//   PROTOCOL → PTZ → PELCO_D; select our address with N+CAM.
//
// ── Debug-first bring-up ────────────────────────────────────────────────────
// {"task":"/ptz_act", "debug":2}  → print every received chunk as raw hex
//                                   (wiring / baud / A-B-swap debugging)
// {"task":"/ptz_act", "debug":1}  → print each decoded frame as JSON
// {"task":"/ptz_act", "debug":0}  → quiet (default)
// {"task":"/ptz_act", "addr":3}   → only accept frames for camera address 3
//                                   (0 = accept any, default)
// {"task":"/ptz_act", "timeout":2000} → motion watchdog ms (0 = off); if no
//                                   frame arrives in this window the router
//                                   stops all axes (lost-stop-frame failsafe)
// {"task":"/ptz_get"}             → parser stats + last frame + motion state
//
// Compiled only when PTZ_KEYBOARD_CONTROLLER is defined
// (see main/config/UC2_canopen_bridge_ptz/PinConfig.h).
#pragma once

#ifdef PTZ_KEYBOARD_CONTROLLER

#include <stdint.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace PtzKeyboard
{
    // Continuous control state, overwritten by every accepted motion frame.
    struct Motion
    {
        int8_t   panDir    = 0;   // -1 left, +1 right
        int8_t   tiltDir   = 0;   // -1 down, +1 up
        uint8_t  panSpeed  = 0;   // 0..63
        uint8_t  tiltSpeed = 0;   // 0..63
        int8_t   zoomDir   = 0;   // +1 tele, -1 wide
        int8_t   focusDir  = 0;   // +1 far, -1 near
        int8_t   irisDir   = 0;   // +1 open, -1 close
        uint32_t updatedMs = 0;   // millis() of the last accepted frame
    };

    // Discrete key event (queue payload). Types match the TPDO2 payload the
    // master decodes in forwardPtzSlaveTpdo (CANopenModule.cpp).
    enum EventType : uint8_t
    {
        EV_NONE         = 0,
        EV_PRESET_CALL  = 1,  // arg = preset number   → e.g. snap image
        EV_PRESET_SET   = 2,
        EV_PRESET_CLEAR = 3,
        EV_AUX_ON       = 4,  // arg = aux number (1..6 on this keyboard)
        EV_AUX_OFF      = 5,
        EV_IRIS_OPEN    = 6,  // rising edge of the OPEN key
        EV_IRIS_CLOSE   = 7,
        EV_CAMERA_ONOFF = 8,
        EV_AUTOSCAN     = 9,
    };

    struct Event
    {
        uint8_t type = EV_NONE;
        uint8_t arg  = 0;
    };

    void setup();                       // UART driver + RX task
    bool getMotion(Motion& out);        // false until the first frame arrived
    QueueHandle_t getEventQueue();      // Event items, depth 16

    uint32_t motionTimeoutMs();         // watchdog config for the router

    // Serial JSON endpoints, dispatched by DeviceRouter (/ptz_act, /ptz_get)
    cJSON* act(cJSON* doc);
    cJSON* get(cJSON* doc);
}

#endif // PTZ_KEYBOARD_CONTROLLER
