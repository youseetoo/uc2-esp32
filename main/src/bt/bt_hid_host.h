#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_log.h"

#include "esp_hidh.h"
#include "esp_hid_gap.h"
#include <BluetoothSerial.h>

static const char *TAG = "ESP_HIDH_DEMO";

class bt_hid_host
{
    public:
    void scanAndConnect(esp_bd_addr_t * mac);
    private:
    static void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
    static void hid_demo_task(esp_bd_addr_t * mac);
    bool esp_bd_addr_t_equal(esp_bd_addr_t adr1, esp_bd_addr_t adr2);
};