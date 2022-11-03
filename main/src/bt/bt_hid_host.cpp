#include "bt_hid_host.h"
#include "esp_log.h"

#define CONFIG_BT_HID_HOST_ENABLED 1
#define CONFIG_BT_BLE_ENABLED 1

void bt_hid_host::scanAndConnect(esp_bd_addr_t *mac)
{
     esp_err_t ret;
#if HID_HOST_MODE == HIDH_IDLE_MODE
    ESP_LOGE(TAG, "Please turn on BT HID host or BLE!");
    return;
#endif
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    /*ESP_ERROR_CHECK( ret );*/
    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
    //ESP_ERROR_CHECK(  );
    esp_hid_gap_init(HID_HOST_MODE);
#if CONFIG_BT_BLE_ENABLED
    //ESP_ERROR_CHECK(  );
    esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler);
#endif /* CONFIG_BT_BLE_ENABLED */
    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_LOGI(TAG, "esp_hidh_init");
    //ESP_ERROR_CHECK(  );
    esp_hidh_init(&config);
    hid_demo_task(mac);
    //xTaskCreate(&hid_demo_task, "hid_task", 6 * 1024, NULL, 2, NULL);
}

bool esp_bd_addr_t_equal(esp_bd_addr_t adr1, esp_bd_addr_t  adr2)
{
	return memcmp(adr1, adr2, 6) == 0;// equals
}

#define SCAN_DURATION_SECONDS 5

void bt_hid_host::hid_demo_task(esp_bd_addr_t *mac)
{
    size_t results_len = 0;
    esp_hid_scan_result_t *results = NULL;
    ESP_LOGI(TAG, "SCAN...");
    //start scan for HID devices
    esp_hid_scan(SCAN_DURATION_SECONDS, &results_len, &results);
    ESP_LOGI(TAG, "SCAN: %u results", results_len);
    if (results_len) {
        esp_hid_scan_result_t *r = results;
        esp_hid_scan_result_t *cr = NULL;
        esp_bd_addr_t _mac;
        while (r && cr == NULL) {
            ESP_LOGI(TAG,"  %s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
            ESP_LOGI(TAG,"RSSI: %d, ", r->rssi);
            ESP_LOGI(TAG,"USAGE: %s, ", esp_hid_usage_str(r->usage));
            bool same = memcmp(r->bda, (*mac), 6) == 0;
            ESP_LOGI(TAG,"same mac: %i, ",same);
#if CONFIG_BT_BLE_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BLE && same) {
                cr = r;
                ESP_LOGI(TAG,"APPEARANCE: 0x%04x, ", r->ble.appearance);
                ESP_LOGI(TAG,"ADDR_TYPE: '%s', ", ble_addr_type_str(r->ble.addr_type));
            }
#endif /* CONFIG_BT_BLE_ENABLED */
#if CONFIG_BT_HID_HOST_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BT && same) {
                cr = r;
                ESP_LOGI(TAG,"COD: %s[", esp_hid_cod_major_str(r->bt.cod.major));
                esp_hid_cod_minor_print(r->bt.cod.minor, stdout);
                ESP_LOGI(TAG,"] srv 0x%03x, ", r->bt.cod.service);
                print_uuid(&r->bt.uuid);
                ESP_LOGI(TAG,", ");
            }
#endif /* CONFIG_BT_HID_HOST_ENABLED */
            ESP_LOGI(TAG,"NAME: %s ", r->name ? r->name : "");
            ESP_LOGI(TAG,"\n");
            r = r->next;
        }
        if (cr  != NULL) {
            //open the last result
            ESP_LOGI(TAG," open NAME: %s ", cr->name ? cr->name : "");
            esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type);
        }
        else
            ESP_LOGI(TAG,"no device found to connect...");
        //free the results
        ESP_LOGI(TAG, "esp_hid_scan_results_free");
        esp_hid_scan_results_free(results);
    }
    vTaskDelete(NULL);
}

void bt_hid_host::hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDH_OPEN_EVENT: {
        if (param->open.status == ESP_OK) {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(TAG, "OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
            esp_hidh_dev_dump(param->open.dev, stdout);
        } else {
            ESP_LOGI(TAG, " OPEN failed!");
        }
        break;
    }
    case ESP_HIDH_BATTERY_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        ESP_LOGI(TAG, "BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        break;
    }
    case ESP_HIDH_INPUT_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
        ESP_LOG_BUFFER_HEX(TAG, param->input.data, param->input.length);
        break;
    }
    case ESP_HIDH_FEATURE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
        ESP_LOGI(TAG, " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d", ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->feature.usage), param->feature.map_index, param->feature.report_id,
                 param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDH_CLOSE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        break;
    }
    default:
        ESP_LOGI(TAG, "EVENT: %d", event);
        break;
    }
}