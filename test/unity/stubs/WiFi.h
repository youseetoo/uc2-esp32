// Host stub: WiFi.h — no-op WiFi surface used by State.cpp (OTA path only).
#pragma once
#include <cstdint>
#include <string>
// State.cpp calls esp_read_mac()/ESP_MAC_WIFI_STA in the OTA path without
// including esp_mac.h itself; the real WiFi.h pulls it in transitively.
#include "esp_mac.h"

class IPAddress {
public:
    IPAddress() {}
    std::string toString() const { return "0.0.0.0"; }
};

class WiFiClass {
public:
    void softAP(const char* ssid) { (void)ssid; }
    void softAPdisconnect(bool plusSleep = false) { (void)plusSleep; }
    void disconnect(bool wifioff = false) { (void)wifioff; }
    IPAddress softAPIP() { return IPAddress(); }
};

extern WiFiClass WiFi;
