// device_router.cpp — Routing-table-driven command dispatcher (T4.4 split).
// routeCommand() maps a JSON task name to the matching handler via strcmp
// chains; the per-command handlers themselves live in serial_dispatch.cpp.
// The extern-"C" shims at the bottom wrap the C++ entry points for the
// pure-C modules (SerialProcessIn.c, MotorGamePad.c, StageScan.c).
// Active on ALL builds. REMOTE dispatch requires CAN_CONTROLLER_CANOPEN.
#include <PinConfig.h>
#include "DeviceRouter.h"

#include "Endpoints.h"
#include "esp_log.h"
#include <cstring>
#ifdef PTZ_KEYBOARD_CONTROLLER
#include "PtzKeyboard.h"
#endif
#ifdef JOYSTICK_USBHOST_PROVIDER
#include "JoystickRouter.h"
#endif

#define TAG "UC2_DR"

// ============================================================================
// Route a task to the correct handler via the routing table
// ============================================================================
cJSON* DeviceRouter::routeCommand(const char* task, cJSON* doc) {
    if (task == nullptr || doc == nullptr) return nullptr;
    log_i("DeviceRouter received task: %s", task);
#ifdef MOTOR_CONTROLLER
    if (strcmp(task, motor_act_endpoint) == 0)
        return handleMotorAct(doc);
    if (strcmp(task, motor_get_endpoint) == 0)
        return handleMotorGet(doc);
#endif
#ifdef LASER_CONTROLLER
    if (strcmp(task, laser_act_endpoint) == 0)
        return handleLaserAct(doc);
    if (strcmp(task, laser_get_endpoint) == 0)
        return handleLaserGet(doc);
#endif
#ifdef HOME_MOTOR
    if (strcmp(task, home_act_endpoint) == 0)
        return handleHomeAct(doc);
    if (strcmp(task, home_get_endpoint) == 0)
        return handleHomeGet(doc);
#endif
#ifdef LED_CONTROLLER
    if (strcmp(task, ledarr_act_endpoint) == 0)
        return handleLedAct(doc);
    if (strcmp(task, ledarr_get_endpoint) == 0)
        return handleLedGet(doc);
#endif
#ifdef GALVO_CONTROLLER
    if (strcmp(task, galvo_act_endpoint) == 0)
        return handleGalvoAct(doc);
    if (strcmp(task, galvo_get_endpoint) == 0)
        return handleGalvoGet(doc);
#endif
#ifdef TMC_CONTROLLER
    if (strcmp(task, tmc_act_endpoint) == 0)
        return handleTmcAct(doc);
    if (strcmp(task, tmc_get_endpoint) == 0)
        return handleTmcGet(doc);
#endif

    // Digital I/O — always routed, even on builds without local DIGITAL_*_CONTROLLER.
    // A master without a local DigitalOutController still needs to forward
    // /digitalout_act to a remote GPIO slave; that's the primary use case.
    if (strcmp(task, digitalout_act_endpoint) == 0)
        return handleDigitalOutAct(doc);
    if (strcmp(task, digitalout_get_endpoint) == 0)
        return handleDigitalOutGet(doc);
    if (strcmp(task, digitalin_get_endpoint) == 0)
        return handleDigitalInGet(doc);

    // GPIO-slave collision detector (local on GPIO slave, SDO on master)
    if (strcmp(task, gpio_act_endpoint) == 0)
        return handleGpioAct(doc);
    if (strcmp(task, gpio_get_endpoint) == 0)
        return handleGpioGet(doc);

    // Generic I2C passthrough on the GPIO slave (local on slave, SDO on master)
    if (strcmp(task, i2c_act_endpoint) == 0)
        return handleI2cAct(doc);
    if (strcmp(task, i2c_get_endpoint) == 0)
        return handleI2cGet(doc);

#ifdef PTZ_KEYBOARD_CONTROLLER
    // PTZ keyboard bridge — debug/config, local only (USB serial on the node)
    if (strcmp(task, ptz_act_endpoint) == 0)
        return PtzKeyboard::act(doc);
    if (strcmp(task, ptz_get_endpoint) == 0)
        return PtzKeyboard::get(doc);
#endif

#ifdef JOYSTICK_USBHOST_PROVIDER
    // DS4 joystick bridge — speed-scaling config, local only (USB serial on the node)
    if (strcmp(task, joystick_act_endpoint) == 0)
        return JoystickRouter::act(doc);
    if (strcmp(task, joystick_get_endpoint) == 0)
        return JoystickRouter::get(doc);
#endif

    // State act — handles "restart" with optional remote nodeId targeting.
    // state_get stays in SerialProcess for now (local State::get also serves
    // the master uptime; remote uptime is reachable via /can_get).
    if (strcmp(task, state_act_endpoint) == 0) // {"task":"/state_act","state":"restart","nodeId":11}
        return handleStateAct(doc);

#if defined(CAN_CONTROLLER_CANOPEN) && !defined(CANOPEN_OTA_NO_SENDER)
    if (strcmp(task, ota_start_endpoint) == 0){
        return handleOtaStart(doc);
    }
#endif

    return nullptr; // not a routed command
}

// T4.1: C shim — pure-C modules (MotorGamePad.c / StageScan.c) dispatch
// /motor_act documents through this.
extern "C" cJSON *device_router_handle_motor_act(cJSON *doc)
{ return DeviceRouter::handleMotorAct(doc); }

extern "C" cJSON *device_router_handle_laser_act(cJSON *doc)
{ return DeviceRouter::handleLaserAct(doc); }

extern "C" cJSON *device_router_handle_led_act(cJSON *doc)
{ return DeviceRouter::handleLedAct(doc); }

extern "C" cJSON *device_router_handle_home_act(cJSON *doc)
{ return DeviceRouter::handleHomeAct(doc); }

// T4.2: generic command dispatcher for the pure-C SerialProcessIn.c
// (jsonProcessor). Wraps DeviceRouter::routeCommand.
extern "C" cJSON *device_router_route_command(const char *task, cJSON *doc)
{ return DeviceRouter::routeCommand(task, doc); }
