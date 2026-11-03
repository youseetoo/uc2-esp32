// device_router.c — Routing-table-driven command dispatcher (T4.4 4f).
// device_router_route_command() maps a JSON task name to the matching handler
// via strcmp chains; the per-command handlers themselves live in
// serial_dispatch.c (C since T4.4 4g, declared in DeviceRouter.h). The
// PTZ / joystick bridges are still C++ namespaces — they are reached
// through the device_router_ptz_* / device_router_joystick_* shims
// defined in serial_dispatch_island.cpp (C++ island).
// Active on ALL builds. REMOTE dispatch requires CAN_CONTROLLER_CANOPEN.
#include <PinConfig.h>
#include "DeviceRouter.h"

#include "Endpoints.h"
#include "esp_log.h"
#include <string.h>

#define TAG "UC2_DR"

// ============================================================================
// Route a task to the correct handler via the routing table
// ============================================================================
cJSON *device_router_route_command(const char* task, cJSON* doc)
{
    if (task == NULL || doc == NULL) return NULL;
    log_i("DeviceRouter received task: %s", task);
#ifdef MOTOR_CONTROLLER
    if (strcmp(task, motor_act_endpoint) == 0)
        return device_router_handle_motor_act(doc);
    if (strcmp(task, motor_get_endpoint) == 0)
        return device_router_handle_motor_get(doc);
#endif
#ifdef LASER_CONTROLLER
    if (strcmp(task, laser_act_endpoint) == 0)
        return device_router_handle_laser_act(doc);
    if (strcmp(task, laser_get_endpoint) == 0)
        return device_router_handle_laser_get(doc);
#endif
#ifdef HOME_MOTOR
    if (strcmp(task, home_act_endpoint) == 0)
        return device_router_handle_home_act(doc);
    if (strcmp(task, home_get_endpoint) == 0)
        return device_router_handle_home_get(doc);
#endif
#ifdef LED_CONTROLLER
    if (strcmp(task, ledarr_act_endpoint) == 0)
        return device_router_handle_led_act(doc);
    if (strcmp(task, ledarr_get_endpoint) == 0)
        return device_router_handle_led_get(doc);
#endif
#ifdef GALVO_CONTROLLER
    if (strcmp(task, galvo_act_endpoint) == 0)
        return device_router_handle_galvo_act(doc);
    if (strcmp(task, galvo_get_endpoint) == 0)
        return device_router_handle_galvo_get(doc);
#endif
#ifdef TMC_CONTROLLER
    if (strcmp(task, tmc_act_endpoint) == 0)
        return device_router_handle_tmc_act(doc);
    if (strcmp(task, tmc_get_endpoint) == 0)
        return device_router_handle_tmc_get(doc);
#endif

    // Digital I/O — always routed, even on builds without local DIGITAL_*_CONTROLLER.
    // A master without a local DigitalOutController still needs to forward
    // /digitalout_act to a remote GPIO slave; that's the primary use case.
    if (strcmp(task, digitalout_act_endpoint) == 0)
        return device_router_handle_digitalout_act(doc);
    if (strcmp(task, digitalout_get_endpoint) == 0)
        return device_router_handle_digitalout_get(doc);
    if (strcmp(task, digitalin_get_endpoint) == 0)
        return device_router_handle_digitalin_get(doc);

    // GPIO-slave collision detector (local on GPIO slave, SDO on master)
    if (strcmp(task, gpio_act_endpoint) == 0)
        return device_router_handle_gpio_act(doc);
    if (strcmp(task, gpio_get_endpoint) == 0)
        return device_router_handle_gpio_get(doc);

    // Generic I2C passthrough on the GPIO slave (local on slave, SDO on master)
    if (strcmp(task, i2c_act_endpoint) == 0)
        return device_router_handle_i2c_act(doc);
    if (strcmp(task, i2c_get_endpoint) == 0)
        return device_router_handle_i2c_get(doc);

#ifdef PTZ_KEYBOARD_CONTROLLER
    // PTZ keyboard bridge — debug/config, local only (USB serial on the node)
    if (strcmp(task, ptz_act_endpoint) == 0)
        return device_router_ptz_act(doc);
    if (strcmp(task, ptz_get_endpoint) == 0)
        return device_router_ptz_get(doc);
#endif

#ifdef JOYSTICK_USBHOST_PROVIDER
    // DS4 joystick bridge — speed-scaling config, local only (USB serial on the node)
    if (strcmp(task, joystick_act_endpoint) == 0)
        return device_router_joystick_act(doc);
    if (strcmp(task, joystick_get_endpoint) == 0)
        return device_router_joystick_get(doc);
#endif

    // State act — handles "restart" with optional remote nodeId targeting.
    // state_get stays in SerialProcess for now (local State::get also serves
    // the master uptime; remote uptime is reachable via /can_get).
    if (strcmp(task, state_act_endpoint) == 0) // {"task":"/state_act","state":"restart","nodeId":11}
        return device_router_handle_state_act(doc);

#if defined(CAN_CONTROLLER_CANOPEN) && !defined(CANOPEN_OTA_NO_SENDER)
    if (strcmp(task, ota_start_endpoint) == 0){
        return device_router_handle_ota_start(doc);
    }
#endif

    return NULL; // not a routed command
}
