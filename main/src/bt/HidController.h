#include <PinConfig.h>
#ifdef BTHID
#pragma once 

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
#include "HidGamePad.h"
#include "esp_hidh.h"
#include "esp_hid_gap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Arduino.h"

static const char *TAG = "HIDGamePad";

extern GamePadData gamePadData;
extern bool hidIsConnected;

void setupHidController();

void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);

#define SCAN_DURATION_SECONDS 5

void hid_demo_task(void *pvParameters);
#endif