#pragma once
#include "WifiConfig.h"
#include "cJSON.h"
#include "Arduino.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_MAXIMUM_RETRY 5
#define WIFI_TIMEOUT_MS 5000 // Timeout in milliseconds (30 seconds)

class EspWifiController
{
    /* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
    private:
        WifiConfig * wconfig;
        const uint8_t DEFAULT_SCAN_LIST_SIZE = 10;
        const char* TAG = "EspWifiController";
        /* FreeRTOS event group to signal when we are connected*/
        void wifi_init_soft_ap();
        void wifi_init_sta();
        char* filter_ssid_starting_with(cJSON *root, const char *prefix);

    public:
        void connect();
        void disconnect();
        void setWifiConfig(WifiConfig * _wconfig){wconfig = _wconfig;}
        cJSON * wifi_scan();
        
};