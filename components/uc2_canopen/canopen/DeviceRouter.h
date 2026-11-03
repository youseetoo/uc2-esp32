// DeviceRouter.h — Routing-table-driven command dispatcher.
// Consults RoutingTable to decide: LOCAL → call controller directly,
// REMOTE → forward via CANopen SDO, DISABLED → log warning.
// Active on all CANopen builds (CAN_CONTROLLER_CANOPEN).
#pragma once

#include <cJSON.h>
#include <stdint.h>

// T4.4 4f: C dispatcher API — the real API. The route chain
// (device_router_route_command) lives in device_router.c; the per-command
// handlers live in serial_dispatch.c (C since step 4g, declared here under
// extern "C"). The C++ class
// below is a thin compatibility layer forwarding to these.
#ifdef __cplusplus
extern "C" {
#endif
// Route a JSON command through the routing table.
// Returns the JSON response (caller must free with cJSON_Delete),
// or NULL if this task is not handled.
cJSON *device_router_route_command(const char *task, cJSON *doc);

// Per-command-type handlers — each consults RoutingTable::find()
cJSON *device_router_handle_motor_act(cJSON *doc);
cJSON *device_router_handle_motor_get(cJSON *doc);
cJSON *device_router_handle_laser_act(cJSON *doc);
cJSON *device_router_handle_laser_get(cJSON *doc);
cJSON *device_router_handle_home_act(cJSON *doc);
cJSON *device_router_handle_home_get(cJSON *doc);
cJSON *device_router_handle_led_act(cJSON *doc);
cJSON *device_router_handle_led_get(cJSON *doc);
cJSON *device_router_handle_galvo_act(cJSON *doc);
cJSON *device_router_handle_galvo_get(cJSON *doc);
cJSON *device_router_handle_tmc_act(cJSON *doc);
cJSON *device_router_handle_tmc_get(cJSON *doc);

// Digital I/O act/get — LOCAL goes to DigitalOutController / DigitalInController,
// REMOTE writes OD x2301 (out) / reads OD x2300 (in) on the requested node.
// The "node" field in the JSON document, when present and we're a master,
// forces a REMOTE dispatch regardless of routing-table state.
cJSON *device_router_handle_digitalout_act(cJSON *doc);
cJSON *device_router_handle_digitalout_get(cJSON *doc);
cJSON *device_router_handle_digitalin_get(cJSON *doc);

// GPIO-slave collision detector. On the GPIO slave itself these call
// GpioCanSlave locally; on a CAN master they forward to the remote node
// (doc "node" key, default pinConfig.MASTER_GPIO_SLAVE_NODE_ID) via SDO:
//   act: threshold->0x2331, sensitivity->0x2332, reference->0x2330,
//        calibrate->0x2333=1
//   get: reads 0x2334/0x2310/0x2330-0x2332/0x2300:04 into {"gpio":{...}}
cJSON *device_router_handle_gpio_act(cJSON *doc);
cJSON *device_router_handle_gpio_get(cJSON *doc);

// Generic I2C passthrough on the GPIO slave. On the slave itself these
// drive I2cBridge's OD directly; on a master they push the command blob to
// the remote node via SDO domain write, pulse the trigger, poll the status,
// and read the response back. JSON:
//   {"task":"/i2c_act","node":60,"addr":68,"write":[253],"read":6,
//    "delay":10,"stop":1,"qid":N}
//     addr = 7-bit I2C address; write = byte array; read = #bytes to read;
//     delay = ms between write and read; stop = STOP after write (default
//     repeated-START). Response: {"node":..,"status":2,"data":[...],"qid":N}
//   {"task":"/i2c_get","node":60} — re-read the last response buffer.
cJSON *device_router_handle_i2c_act(cJSON *doc);
cJSON *device_router_handle_i2c_get(cJSON *doc);

// State act — currently only routes "restart" to a remote node when
// "nodeId" is provided in the JSON; other state ops are handled locally.
cJSON *device_router_handle_state_act(cJSON *doc);

#ifdef CAN_CONTROLLER_CANOPEN
// /ota_start — JSON preamble that flips the master into binary OTA receive
// mode. Subsequent raw bytes on Serial are streamed into a chunk buffer
// by ota_processBytes() and then forwarded to the slave via the
// file-static streaming loop in canopen_ota.c. Defined only when
// CANOPEN_OTA_NO_SENDER is not set (matches the definition in
// serial_dispatch.c).
#ifndef CANOPEN_OTA_NO_SENDER
cJSON *device_router_handle_ota_start(cJSON *doc);
#endif
#endif

// T4.4 4f: C shims for the still-C++ PTZ / joystick bridges — device_router.c
// (C) cannot call the PtzKeyboard:: / JoystickRouter:: namespaces directly.
// Defined in serial_dispatch_island.cpp (C++ island); drop once those
// bridges are C-ified (4i).
#ifdef PTZ_KEYBOARD_CONTROLLER
cJSON *device_router_ptz_act(cJSON *doc);
cJSON *device_router_ptz_get(cJSON *doc);
#endif
#ifdef JOYSTICK_USBHOST_PROVIDER
cJSON *device_router_joystick_act(cJSON *doc);
cJSON *device_router_joystick_get(cJSON *doc);
#endif
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// T4.4 4f: C++ compatibility layer — every member is a thin inline forwarder
// to the C API above (behavior-identical to the old class methods).
class DeviceRouter {
public:
    static inline cJSON* routeCommand(const char* task, cJSON* doc) { return device_router_route_command(task, doc); }

    static inline cJSON* handleMotorAct(cJSON* doc) { return device_router_handle_motor_act(doc); }
    static inline cJSON* handleMotorGet(cJSON* doc) { return device_router_handle_motor_get(doc); }
    static inline cJSON* handleLaserAct(cJSON* doc) { return device_router_handle_laser_act(doc); }
    static inline cJSON* handleLaserGet(cJSON* doc) { return device_router_handle_laser_get(doc); }
    static inline cJSON* handleHomeAct(cJSON* doc) { return device_router_handle_home_act(doc); }
    static inline cJSON* handleHomeGet(cJSON* doc) { return device_router_handle_home_get(doc); }
    static inline cJSON* handleLedAct(cJSON* doc) { return device_router_handle_led_act(doc); }
    static inline cJSON* handleLedGet(cJSON* doc) { return device_router_handle_led_get(doc); }
    static inline cJSON* handleGalvoAct(cJSON* doc) { return device_router_handle_galvo_act(doc); }
    static inline cJSON* handleGalvoGet(cJSON* doc) { return device_router_handle_galvo_get(doc); }
    static inline cJSON* handleTmcAct(cJSON* doc) { return device_router_handle_tmc_act(doc); }
    static inline cJSON* handleTmcGet(cJSON* doc) { return device_router_handle_tmc_get(doc); }

    static inline cJSON* handleDigitalOutAct(cJSON* doc) { return device_router_handle_digitalout_act(doc); }
    static inline cJSON* handleDigitalOutGet(cJSON* doc) { return device_router_handle_digitalout_get(doc); }
    static inline cJSON* handleDigitalInGet(cJSON* doc) { return device_router_handle_digitalin_get(doc); }

    static inline cJSON* handleGpioAct(cJSON* doc) { return device_router_handle_gpio_act(doc); }
    static inline cJSON* handleGpioGet(cJSON* doc) { return device_router_handle_gpio_get(doc); }

    static inline cJSON* handleI2cAct(cJSON* doc) { return device_router_handle_i2c_act(doc); }
    static inline cJSON* handleI2cGet(cJSON* doc) { return device_router_handle_i2c_get(doc); }

    static inline cJSON* handleStateAct(cJSON* doc) { return device_router_handle_state_act(doc); }

#ifdef CAN_CONTROLLER_CANOPEN
#ifndef CANOPEN_OTA_NO_SENDER
    static inline cJSON* handleOtaStart(cJSON* doc) { return device_router_handle_ota_start(doc); }
#endif
#endif
};
#endif // __cplusplus
