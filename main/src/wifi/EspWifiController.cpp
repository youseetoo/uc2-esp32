#include "EspWifiController.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/timers.h"

static EventGroupHandle_t s_wifi_event_group;
int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            log_e("retry to connect to the AP");
        } else {
            if (s_wifi_event_group != NULL) {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
        }
        log_e("connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        log_i("got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        if (s_wifi_event_group != NULL) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    } else {
        log_i("event_base: %s event_id: %d", event_base, event_id);
    }
}

void EspWifiController::wifi_init_soft_ap() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config)); // Ensure wifi_config is zeroed
    strncpy((char*)wifi_config.ap.ssid, wconfig->mSsid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char*)wifi_config.ap.password, wconfig->pw, sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.ssid_len = strlen(wconfig->mSsid);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    if (strlen(wconfig->pw) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t sta_config = {0};
    strcpy((char *)sta_config.sta.ssid, wconfig->mSsid);
    strcpy((char *)sta_config.sta.password, wconfig->pw);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    log_i("wifi_init_softap finished. SSID:%s password:%s", wconfig->mSsid, wconfig->pw);
}

void wifi_timer_callback(TimerHandle_t xTimer) {
    if (s_wifi_event_group != NULL) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
}

void EspWifiController::wifi_init_sta() {
    TimerHandle_t s_wifi_timer;
    s_wifi_event_group = xEventGroupCreate();

    if (s_wifi_event_group == NULL) {
        log_e("Failed to create event group");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    cJSON *wifi_scan_results = wifi_scan();

    if (wifi_scan_results) {
        const char *prefix = "UC2xSeeed";
        char *filtered_ssids = filter_ssid_starting_with(wifi_scan_results, prefix);
        if (filtered_ssids == NULL) {
            log_i("No SSID found starting with %s", prefix);
        } else {
            log_i("Filtered SSIDs: %s", filtered_ssids);
            strncpy(wconfig->mSsid, filtered_ssids, sizeof(wconfig->mSsid) - 1);
            free(filtered_ssids);
        }
    }

    if (strlen(wconfig->pw) == 0) {
        log_i("No password set for Wi-Fi network %s", wconfig->mSsid);
        strncpy((char*)wifi_config.sta.ssid, wconfig->mSsid, sizeof(wifi_config.sta.ssid) - 1);
        wifi_config.sta.password[0] = '\0';
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        strncpy((char*)wifi_config.sta.ssid, wconfig->mSsid, sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, wconfig->pw, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    log_i("wifi_init_sta finished.");

    s_wifi_timer = xTimerCreate("wifi_timer", pdMS_TO_TICKS(WIFI_TIMEOUT_MS), pdFALSE, (void *)0, wifi_timer_callback);
    if (s_wifi_timer == NULL) {
        log_e("Failed to create timer");
        return;
    }
    xTimerStart(s_wifi_timer, 0);

    log_i("Waiting for Wi-Fi");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    log_d("bits: %d", bits);
    if (bits & WIFI_CONNECTED_BIT) {
        log_i("Connected to AP SSID:%s", wconfig->mSsid);
    } else if (bits & WIFI_FAIL_BIT) {
        log_i("Failed to connect to SSID:%s", wconfig->mSsid);
    } else {
        log_i("UNEXPECTED EVENT");
    }

    xTimerStop(s_wifi_timer, 0);
    xTimerDelete(s_wifi_timer, 0);
}

void EspWifiController::connect() {
    if (wconfig->ap) {
        log_i("wifi init softap");
        wifi_init_soft_ap();
    } else {
        log_i("wifi init sta");
        wifi_init_sta();
    }
}

void EspWifiController::disconnect() {
    log_i("wifi disconnect");
    esp_wifi_disconnect();
    log_i("wifi deinit");
    esp_wifi_deinit();
    log_i("netif deinit");
    esp_netif_deinit();
}

cJSON *EspWifiController::wifi_scan() {
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    log_i("Total Wifi Stations scanned = %u", ap_count);
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        log_i("SSID \t\t%s", ap_info[i].ssid);
        log_i("RSSI \t\t%d", ap_info[i].rssi);
        log_i("Channel \t\t%d", ap_info[i].primary);
        cJSON_AddItemToArray(root, cJSON_CreateString((char *)ap_info[i].ssid));
    }
    return root;
}

char* EspWifiController::filter_ssid_starting_with(cJSON *root, const char *prefix) {
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, root) {
        const char *ssid = cJSON_GetStringValue(item);
        if (ssid && strncmp(ssid, prefix, strlen(prefix)) == 0) {
            char *result = strdup(ssid);
            cJSON_Delete(root);
            return result;
        }
    }

    cJSON_Delete(root);
    return NULL;
}
