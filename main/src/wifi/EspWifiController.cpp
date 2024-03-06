#include "EspWifiController.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

// #include "lwip/err.h"
// #include "lwip/sys.h"

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		log_i("WIFI_EVENT_STA_DISCONNECTED");
		esp_wifi_connect();
		xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		log_i("IP_EVENT_STA_GOT_IP");
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void EspWifiController::initSoftAp()
{
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
    memcpy(wifi_config.ap.ssid, wconfig->mSsid, 32);
    memcpy(wifi_config.ap.password, wconfig->pw, 64);
    /*wconfig->mSsid.getBytes(wifi_config.ap.ssid,32);
    wifi_config.ap.ssid = (uint8_t*) wconfig->mSsid;
    wifi_config.ap.password = (uint8_t*) wconfig->pw;*/
    wifi_config.ap.ssid_len = strlen(wconfig->mSsid);
    wifi_config.ap.channel = 1;

    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    if (strlen(wconfig->pw)==0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t sta_config = { 0 };
	strcpy((char *)sta_config.sta.ssid, wconfig->mSsid);
	strcpy((char *)sta_config.sta.password, wconfig->pw);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    log_i("wifi_init_softap finished. SSID:%s password:%s",
          wconfig->mSsid, wconfig->pw);
    
}

void EspWifiController::wifi_init_sta()
{
    s_wifi_event_group = xEventGroupCreate();

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

    wifi_config_t wifi_config;
    memcpy(wifi_config.ap.ssid, wconfig->mSsid, 32);
    memcpy(wifi_config.ap.password, wconfig->pw, 64);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    log_i("wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    log_i("Waiting for wifi");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    log_d("bits: %d", bits);
    if (bits & WIFI_CONNECTED_BIT)
    {
        log_i("connected to ap SSID:%s password:%s",
              wconfig->mSsid, wconfig->pw);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        log_i("Failed to connect to SSID:%s, password:%s",
              wconfig->mSsid, wconfig->pw);
    }
    else
    {
        log_i("UNEXPECTED EVENT");
    }
}

void EspWifiController::connect()
{
    if (wconfig->ap)
    {
        initSoftAp();
    }
    else
        wifi_init_sta();
}

void EspWifiController::disconnect()
{
    log_i("wifi disconnect");
    esp_wifi_disconnect();
    log_i("wifi deinit");
    esp_wifi_deinit();
    log_i("netif deinit");
    esp_netif_deinit();
}

cJSON *EspWifiController::wifi_scan()
{
    /*ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));*/

    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    //ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    cJSON *root = cJSON_CreateObject();
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++)
    {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        cJSON_AddItemToArray(root, cJSON_CreateString((char *)ap_info[i].ssid));
        ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
    }
    return root;
}