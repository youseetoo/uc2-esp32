// PtzKeyboard.h — RS-485 PTZ keyboard receiver.
//
// Reads Pelco-D/P frames from a CCTV joystick keyboard via RS485↔TTL on UART1
// and exposes:
//   * a thread-safe Motion snapshot (pan/tilt speeds, zoom/focus/iris) that
//     PtzRouter polls at 50 Hz, and
//   * a small event queue for discrete keys (preset set/call/clear, AUX n)
//     that PtzRouter forwards to the CAN master.
//
// Wiring (adapter with automatic direction control, e.g. MAX13487 boards):
//   keyboard A/B → adapter A/B
//   adapter RXD → PTZ_RS485_RX, adapter TXD → PTZ_RS485_TX (unused, we only listen)
//   keyboard menu: 995+AUX → BAUDRATE 9600 → PROTOCOL → PELCO_D; N+CAM for addr
//
// Debug via /ptz_act:
//   {"task":"/ptz_act","debug":2}  → print every received chunk as raw hex
//   {"task":"/ptz_act","debug":1}  → print each decoded frame as JSON
//   {"task":"/ptz_act","addr":3}   → only accept frames for camera address 3
//   {"task":"/ptz_act","timeout":2000} → motion watchdog ms (0 = off)
//   {"task":"/ptz_get"}            → parser stats + last frame + motion state
//
// T4.4 4i: C-ified (was namespace PtzKeyboard).

#ifndef PTZ_KEYBOARD_H
#define PTZ_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Continuous control state, overwritten by every accepted motion frame.
typedef struct {
    int8_t    panDir;       // -1 left, +1 right
    int8_t    tiltDir;      // -1 down, +1 up
    uint8_t   panSpeed;     // 0..63
    uint8_t   tiltSpeed;    // 0..63
    int8_t    zoomDir;      // +1 tele, -1 wide
    int8_t    focusDir;     // +1 far, -1 near
    int8_t    irisDir;      // +1 open, -1 close
    uint32_t  updatedMs;    // millis() of the last accepted frame
} ptz_motion_t;

// Discrete key event (queue payload). Types match the TPDO2 payload the
// master decodes in forwardPtzSlaveTpdo.
typedef enum {
    PTZ_EV_NONE         = 0,
    PTZ_EV_PRESET_CALL  = 1,  // arg = preset number
    PTZ_EV_PRESET_SET   = 2,
    PTZ_EV_PRESET_CLEAR = 3,
    PTZ_EV_AUX_ON       = 4,  // arg = aux number (1..6)
    PTZ_EV_AUX_OFF      = 5,
    PTZ_EV_IRIS_OPEN    = 6,  // rising edge of the OPEN key
    PTZ_EV_IRIS_CLOSE   = 7,
    PTZ_EV_CAMERA_ONOFF = 8,
    PTZ_EV_AUTOSCAN     = 9,
} ptz_event_type_t;

typedef struct {
    uint8_t type;
    uint8_t arg;
} ptz_event_t;

#ifdef __cplusplus
extern "C" {
#endif

void ptz_keyboard_setup(void);
bool ptz_keyboard_get_motion(ptz_motion_t* out);
QueueHandle_t ptz_keyboard_get_event_queue(void);
uint32_t ptz_keyboard_motion_timeout_ms(void);
cJSON* ptz_keyboard_act(cJSON* doc);
cJSON* ptz_keyboard_get(cJSON* doc);

#ifdef __cplusplus
}
#endif

#endif // PTZ_KEYBOARD_H