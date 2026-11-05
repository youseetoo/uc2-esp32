#pragma once
// T4.5: C API for the HID controller layer. Replaces the C++ HidController header.
// Declares the gamepad data structures and the HID controller lifecycle functions.

#include <stdbool.h>
#include "HidGamePad.h"

#ifdef ESP_PLATFORM
#include "esp_event.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Global gamepad state (shared between HidController and BtController)
extern GamePadData gamePadData;
extern bool hidIsConnected;

// HID controller lifecycle
void setupHidController(void);

// HID scan task (created as FreeRTOS task for background scanning)
// On non-ESP platforms (host tests), this is a no-op declaration.
void hid_demo_task(void *pvParameters);

// HID event callback (ESP-IDF event loop)
#ifdef ESP_PLATFORM
void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
#endif

// Input event handling
void updateGamePadDataDS4(const DS4Data *d);
void updateGamePadDataHyperX(const HyperXClutchData *d);
void handleHidInputEvent(void *param);

#ifdef __cplusplus
}
#endif
