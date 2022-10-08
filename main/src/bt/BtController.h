#pragma once

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include"esp_gap_bt_api.h"
#include "esp_err.h"
#include <ArduinoJson.h>
#include "../../config.h"

namespace BtController
{
    void setup();
    void scanForDevices(DynamicJsonDocument * jdoc);
    void notifyCallback(
        BLERemoteCharacteristic *pBLERemoteCharacteristic,
        uint8_t *pData,
        size_t length,
        bool isNotify);
    void setMacAndConnect(String m);
    void removePairedDevice(String pairedmac);
    BLEScanResults scanAndGetResult(BLEScan *pBLEScan);
    void getPairedDevices(DynamicJsonDocument *jdoc);
    char * bda2str(const uint8_t *bda, char *str, size_t size);
    bool connectToServer();
    void my_gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

    void my_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gatts_cb_param_t *param);

    void my_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

    
}