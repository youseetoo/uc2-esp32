#include "HidController.h"

GamePadData gamePadData;
bool hidIsConnected = false;

void setupHidController()
{
    
ESP_LOGI(TAG, "setup");
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
    ESP_ERROR_CHECK( ret );
    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
    ESP_ERROR_CHECK( esp_hid_gap_init(HID_HOST_MODE) );
#if CONFIG_BT_BLE_ENABLED
    ESP_ERROR_CHECK( esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler) );
#endif /* CONFIG_BT_BLE_ENABLED */
    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK( esp_hidh_init(&config) );
    //hid_demo_task();
    xTaskCreatePinnedToCore(&hid_demo_task, "hid_task",8192, NULL, 5, NULL,1);
}

int slowInputLog = 0;

void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDH_OPEN_EVENT: {
        if (param->open.status == ESP_OK) {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
            esp_hidh_dev_dump(param->open.dev, stdout);
            hidIsConnected = true;
        } else {
            ESP_LOGE(TAG, " OPEN failed!");
        }
        break;
    }
    case ESP_HIDH_BATTERY_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        break;
    }
    case ESP_HIDH_INPUT_EVENT: {
        if (param->input.length == 15)
        {
            HyperXClutchData * d = (HyperXClutchData*)param->input.data;
            gamePadData.circle = d->Buttons.B;
            gamePadData.cross = d->Buttons.A;
            gamePadData.triangle = d->Buttons.Y;
            gamePadData.square = d->Buttons.X;
            gamePadData.l1 = d->Buttons.L1;
            gamePadData.l2 = d->Buttons.L2;
            gamePadData.r1 = d->Buttons.R1;
            gamePadData.r2 = d->Buttons.R2;

            gamePadData.LeftX = d->LeftX;
            gamePadData.LeftY = d->LeftY;
            gamePadData.RightX = d->RightX;
            gamePadData.RightY = d->RightY;

            gamePadData.dpaddirection = static_cast<Dpad::Direction>(d->Dpad);
        }
        else if (param->input.length == 9)
        {
            DS4Data * d = (DS4Data*)param->input.data;
            gamePadData.circle = d->Buttons.circle;
            gamePadData.cross = d->Buttons.cross;
            gamePadData.triangle = d->Buttons.triangle;
            gamePadData.square = d->Buttons.square;
            gamePadData.l1 = d->Buttons.l1;
            gamePadData.l2 = d->Buttons.l2;
            gamePadData.r1 = d->Buttons.r1;
            gamePadData.r2 = d->Buttons.r2;

            gamePadData.LeftX = d->LeftX;
            gamePadData.LeftY = d->LeftY;
            gamePadData.RightX = d->RightX;
            gamePadData.RightY = d->RightY;
            if (d->Buttons.dpad == 0)
            {
                gamePadData.dpaddirection = Dpad::Direction::none;
            }
            
            if (d->Buttons.dpad == 16)
            {
                gamePadData.dpaddirection = Dpad::Direction::up;
            }
            if (d->Buttons.dpad == 32)
            {
                gamePadData.dpaddirection = Dpad::Direction::right;
            }
            if (d->Buttons.dpad == 64)
            {
                gamePadData.dpaddirection = Dpad::Direction::down;
            }
            if (d->Buttons.dpad == 128)
            {
                gamePadData.dpaddirection = Dpad::Direction::left;
            }
        }
        
        
        /*const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        HyperXClutchData * d = (HyperXClutchData*)param->input.data;
        if(slowInputLog == 200)
        {
            ESP_LOGI(TAG,"A:%d, B:%d, X:%d, Y:%d, L1:%d, L2:%d, R1:%d, R2:%d, Start:%d, Select:%d, Home:%d" , (*d).Buttons.A, d->Buttons.B,d->Buttons.X,d->Buttons.Y,d->Buttons.L1,d->Buttons.L2,d->Buttons.R1,d->Buttons.R2,d->Buttons.Start,d->Buttons.Select,d->Buttons.Home);
            ESP_LOGI(TAG,"leftX:%d, leftY:%d, rightX:%d, rightY:%d", d->LeftX, d->LeftY,d->RightX,d->RightY);
            ESP_LOGI(TAG, "Dpad direction:%d", d->Dpad);
            //ESP_LOGI(TAG,"up:%d, down:%d, left:%d, right:%d, up_left:%d, up_right:%d, down_left:%d, down_right:%d", d->Dpad.up, d->Dpad.down, d->Dpad.left, d->Dpad.right, d->Dpad.up_left, d->Dpad.up_right, d->Dpad.down_left,d->Dpad.down_right);
            //ESP_LOGI(TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
            //ESP_LOG_BUFFER_HEX(TAG, param->input.data, param->input.length);
            slowInputLog = 0;
        }
        else
            slowInputLog++;*/
        break;
    }
    case ESP_HIDH_FEATURE_EVENT: {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d", ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->feature.usage), param->feature.map_index, param->feature.report_id,
                 param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDH_CLOSE_EVENT: {
        hidIsConnected = false;
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        break;
    }
    default:
        ESP_LOGI(TAG, "EVENT: %d", event);
        break;
    }
}

void hid_demo_task(void *pvParameters)
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
        while (r) {
            printf("  %s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
            printf("RSSI: %d, ", r->rssi);
            printf("USAGE: %s, ", esp_hid_usage_str(r->usage));
#if CONFIG_BT_BLE_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BLE) {
                cr = r;
                printf("APPEARANCE: 0x%04x, ", r->ble.appearance);
                printf("ADDR_TYPE: '%s', ", ble_addr_type_str(r->ble.addr_type));
            }
#endif /* CONFIG_BT_BLE_ENABLED */
#if CONFIG_BT_HID_HOST_ENABLED
            if (r->transport == ESP_HID_TRANSPORT_BT) {
                cr = r;
                printf("COD: %s[", esp_hid_cod_major_str(r->bt.cod.major));
                esp_hid_cod_minor_print(r->bt.cod.minor, stdout);
                printf("] srv 0x%03x, ", r->bt.cod.service);
                print_uuid(&r->bt.uuid);
                printf(", ");
            }
#endif /* CONFIG_BT_HID_HOST_ENABLED */
            printf("NAME: %s ", r->name ? r->name : "");
            printf("\n");
            r = r->next;
        }
        if (cr) {
            //open the last result
            ESP_LOGI(TAG, "connect...");
            esp_hidh_dev_t * dev = esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type);
            
            ESP_LOGI(TAG, "connected...");
            printf("esp_hidh_dev_open returned %d\n", dev);
        }
        //free the results
        esp_hid_scan_results_free(results);
    }
    ESP_LOGI(TAG, "vtaskdelete hid_demo_task");
    vTaskDelete(NULL);
}

