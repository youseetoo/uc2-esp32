// JoystickUsbHost.h — DualShock 4 over USB-OTG HID host.
//
// Spawns USB Host + HID-Host background tasks (ESP32-S3 only) and decodes the
// 64-byte DS4 boot-style HID report. Decoded state is published into a
// FreeRTOS queue that JoystickRouter consumes.
//
// Build is gated on JOYSTICK_USBHOST_PROVIDER. The implementation references
// the vendored Espressif `usb_host_hid` driver under lib/esp_usb_host_hid/.
#pragma once

#include <stdint.h>

#ifdef JOYSTICK_USBHOST_PROVIDER

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace JoystickUsbHost
{
    // Decoded DS4 state, written by hid_host_generic_report_callback and
    // consumed by JoystickRouter.
    struct Ds4State
    {
        int16_t lx;        // left stick X,  scaled (uint8 - 128) << 8
        int16_t ly;        // left stick Y
        int16_t rx;        // right stick X
        int16_t ry;        // right stick Y
        uint8_t l2;        // left trigger pressure 0..255
        uint8_t r2;        // right trigger pressure 0..255
        uint8_t dpad;      // 0..7 = dir, 8 = released
        uint8_t button1;   // Triangle[7], Circle[6], Cross[5], Square[4]
        uint8_t button2;   // R3:7,L3:6,Options:5,Share:4,R2:3,L2:2,R1:1,L1:0
        uint8_t button3;   // tpad click[1], logo[0]
        bool    valid;     // false on disconnect to tell consumer to stop motors
    };

    // Start the USB Host + HID Host background tasks and create the state
    // queue. Safe to call only once.
    bool begin();

    // Returns the queue handle so the router can xQueuePeek/Receive it.
    QueueHandle_t getQueue();
}

#endif // JOYSTICK_USBHOST_PROVIDER
