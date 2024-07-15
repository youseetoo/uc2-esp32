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
int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            log_e("retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        log_e("connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        log_i("got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    else{
        log_i("event_base: %s event_id: %d", event_base, event_id);
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
    if (strlen(wconfig->pw) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config_t sta_config = {0};
    strcpy((char *)sta_config.sta.ssid, wconfig->mSsid);
    strcpy((char *)sta_config.sta.password, wconfig->pw);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    log_i("wifi_init_softap finished. SSID:%s password:%s",
          wconfig->mSsid, wconfig->pw);
    log_i("Current IP address: %s", "1");
}

#include "freertos/timers.h"


void wifi_timer_callback(TimerHandle_t xTimer)
{
    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
}

void EspWifiController::wifi_init_sta()
{
    TimerHandle_t s_wifi_timer;
    /*
    This connects the ESP32 to an external Wi-Fi network provided by the
    mSSID and the PW in the pinconfig.h.
    */
    // Create an event group to manage Wi-Fi events
    s_wifi_event_group = xEventGroupCreate();

    // Initialize the underlying TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create a default Wi-Fi station interface
    esp_netif_create_default_wifi_sta();

    // Default Wi-Fi configuration initialization
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register Wi-Fi event handlers
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


    // Configure the Wi-Fi connection settings
    wifi_config_t wifi_config = {};

    // Set the Wi-Fi mode to station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Set the Wi-Fi configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // Start the Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Scan available networks
    cJSON *wifi_scan_results = wifi_scan();

    if (wifi_scan_results)
    {
        // Check if we are connected to a specific network and use this for the connection
        const char *prefix = "UC2xSeeed";
        char *filtered_ssids = filter_ssid_starting_with(wifi_scan_results, prefix);
        if (filtered_ssids == NULL)
        {
            log_i("No SSID found starting with %s", prefix);
         
        }
        else{
            log_i("Filtered SSIDs: %s", filtered_ssids);
            wconfig->mSsid = filtered_ssids;
        }
    }


    // Check if the password is empty or null to determine if it's an open network
    if (strlen(wconfig->pw) == 0)
    {
        log_i("No password set for Wi-Fi network %s", wconfig->mSsid);
        strcpy((char*)wifi_config.sta.ssid, wconfig->mSsid);
        strcpy((char*)wifi_config.sta.password, "\0");
        log_i("Connecting to Wi-Fi network %s", wifi_config.sta.ssid); 
        log_i("Password: %s", wifi_config.sta.password);

        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

    }
    else
    {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; // WPA2-PSK network
        strcpy((char*)wifi_config.sta.ssid, wconfig->mSsid); // Set the SSID
        strcpy((char*)wifi_config.sta.password, wconfig->pw); // Set the password
        //memcpy(wifi_config.sta.password, wconfig->pw, sizeof(wifi_config.sta.password)); // Set the password
    }
    

    // Additional Wi-Fi configuration settings
    //wifi_config.sta.pmf_cfg.capable = true;
    //wifi_config.sta.pmf_cfg.required = false;


    // Set the Wi-Fi configuration
    ESP_ERROR_CHECK(esp_wifi_stop()); // first stop again
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    log_i("Connecting to Wi-Fi network %s", wifi_config.sta.ssid);
    log_i("Password: %s", wifi_config.sta.password);
    // Start the Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());
    log_i("wifi_init_sta finished.");

    // Create a timer for the connection timeout
    s_wifi_timer = xTimerCreate("wifi_timer", pdMS_TO_TICKS(WIFI_TIMEOUT_MS), pdFALSE, (void *)0, wifi_timer_callback);
    if (s_wifi_timer == NULL)
    {
        log_e("Failed to create timer");
        return;
    }
    xTimerStart(s_wifi_timer, 0);



    // Wait until the connection is established or failed
    log_i("Waiting for Wi-Fi");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    // Check which event happened
    log_d("bits: %d", bits);
    if (bits & WIFI_CONNECTED_BIT)
    {
        log_i("Connected to AP SSID:%s", wconfig->mSsid);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        log_i("Failed to connect to SSID:%s", wconfig->mSsid);
        // Retry logic can be implemented here if needed
    }
    else
    {
        log_i("UNEXPECTED EVENT");
    }

    // Stop and delete the timer
    xTimerStop(s_wifi_timer, 0);
    xTimerDelete(s_wifi_timer, 0);
}


void EspWifiController::connect()
{
    if (wconfig->ap)
    {
        log_i("wifi init softap");
        initSoftAp();
    }
    else
    {
        log_i("wifi init sta");
        wifi_init_sta();
    }
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

    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total Wifi Stations scanned = %u", ap_count);
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




char* EspWifiController::filter_ssid_starting_with(cJSON *root, const char *prefix)
{
    cJSON *item = NULL;

    // Iterate through each SSID in the root array
    cJSON_ArrayForEach(item, root)
    {
        const char *ssid = cJSON_GetStringValue(item);
        if (ssid && strncmp(ssid, prefix, strlen(prefix)) == 0)
        {
            // Duplicate the matching SSID to return it
            char *result = strdup(ssid);

            // Clean up the JSON objects
            cJSON_Delete(root);
            return result;  // The caller is responsible for freeing this memory
        }
    }

    // Clean up the JSON objects
    cJSON_Delete(root);
    return NULL;
}


