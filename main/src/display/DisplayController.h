#pragma once
#include <PinConfig.h>
#include "cJSON.h"
#include <stdint.h>

// DisplayController
// ---------------------------------------------------------------------------
// Status OLED for the CAN-HAT v2 master: a 0.91" 128x32 SSD1306 module on the
// shared I2C bus (GPIO21/22, address 0x3C, or 0x3D on some modules).
//
// What it shows
//   * Boot        : "openUC2" + the firmware version, for SPLASH_MS (10 s).
//   * Afterwards  : big title line  = device name pushed by the host
//                   (hostname == Wi-Fi SSID of the Raspberry Pi), "openuc2"
//                   until a name has been set;
//                   status line     = free text (e.g. the running experiment step);
//                   bottom line     = firmware version, or a progress bar while
//                   a progress value is set.
//   * Text wider than the display scrolls as a marquee (done here on the ESP,
//     so the host sends each string once).
//
// Wire protocol (all keys optional, combine freely):
//   {"task":"/display_act","display":{"name":"openuc2-metal-slope-23501"}}  // persisted in NVS, ""=back to "openuc2"
//   {"task":"/display_act","display":{"status":"Scanning 12/340"}}          // "" clears
//   {"task":"/display_act","display":{"progress":37}}                       // 0..100, -1 hides the bar
//   {"task":"/display_act","display":{"on":0}}                              // panel + charge pump off, persisted
//   {"task":"/display_act","display":{"flip":1}}                            // rotate 180 degrees, persisted
//   {"task":"/display_act","display":{"contrast":40}}                       // 0..255, persisted
//   {"task":"/display_act","display":{"splash":1}}                          // replay the boot/firmware screen
//   {"task":"/display_get"} -> {"display":{present,on,addr,name,status,progress,fw,flip,contrast,splash}}
//
// The display is optional: when it does not answer on the bus the module stays
// idle (display_get reports present:0) and probes again every PROBE_INTERVAL_MS,
// so it can be plugged in later without a reboot.
namespace DisplayController
{
    static constexpr uint8_t  ADDR_PRIMARY      = 0x3C;
    static constexpr uint8_t  ADDR_SECONDARY    = 0x3D;
    static constexpr uint32_t SPLASH_MS         = 10000;  // firmware-version screen after power-up
    static constexpr uint32_t PROBE_INTERVAL_MS = 10000;  // re-probe while no display answers
    static constexpr uint32_t I2C_FLUSH_HZ      = 400000; // bus speed while a frame is pushed ...
    static constexpr uint32_t I2C_IDLE_HZ       = 100000; // ... restored afterwards (TMP102/MCP4017 default)

    void setup();
    void loop();

    int    act(cJSON *root);
    cJSON *get(cJSON *root);
}
