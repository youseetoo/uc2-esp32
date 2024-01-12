#include "RestApiCallbacks.h"
#include "../bt/BtController.h"
#include "../image/ImageController.h"
#include "Endpoints.h"

#define SCRATCH_BUFSIZE (4096)//(10240)

typedef struct rest_server_context
{
    char base_path[16];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

namespace RestApi
{
    char output[1000];

    void resetNvFLash()
    {
        int erase = nvs_flash_erase(); // erase the NVS partition and...
        int init = nvs_flash_init();   // initialize the NVS partition.
        log_i("erased:%s init:%s", erase, init);
        delay(500);
    }

    cJSON *deserializeESP(httpd_req_t *req)
    {
        // print req specs
        int total_req_len = req->content_len;
        int cur_len = 0;
        char buf[total_req_len];
        int received = 0;
        if (total_req_len >= SCRATCH_BUFSIZE)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
            return NULL;
        }
        while (cur_len < total_req_len)
        {
            received = httpd_req_recv(req, buf + cur_len, total_req_len);
            if (received <= 0)
            {
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
                return NULL;
            }
            cur_len += received;
        }
        cJSON *doc = NULL;
        if(total_req_len > 0)
            doc = cJSON_Parse(buf);
        return doc;
    }

    void serializeESP(cJSON *doc, httpd_req_t *req)
    {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_type(req, "application/json");
        char * s = cJSON_Print(doc);
        if(s == nullptr){
            log_e("ERROR NULLPTR");
            return;
        }
        // print buffer size of s
        httpd_resp_send(req, s, strlen(s));
        free(s);
    }

    void serializeESP(int doc, httpd_req_t *req)
    {
        httpd_resp_send_chunk(req, NULL, 0);
    }

    /*void update()
    {
        WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
        w->getServer()->sendHeader("Connection", "close");
        w->getServer()->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }

    void upload()
    {
        WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
        HTTPUpload &upload = w->getServer()->upload();
        if (upload.status == UPLOAD_FILE_START)
        {
            log_i("Update: %s\n", upload.filename.c_str());
            if (!Update.begin(UPDATE_SIZE_UNKNOWN))
            { // start with max available size
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_WRITE)
        {
            //flashing firmware to ESP
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
            {
                Update.printError(Serial);
            }
        }
        else if (upload.status == UPLOAD_FILE_END)
        {
            if (Update.end(true))
            { // true to set the size to the current progress
                log_i("Update Success: %u\nRebooting...\n", upload.totalSize);
            }
            else
            {
                Update.printError(Serial);
            }
        }
    }*/

    esp_err_t getEndpoints(httpd_req_t *req)
    {
        cJSON *doc = cJSON_CreateArray();
        cJSON_AddItemToArray(doc, cJSON_CreateString(ota_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(update_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(identity_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(scanwifi_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(connectwifi_endpoint));

        if (moduleController.get(AvailableModules::laser) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(laser_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(laser_get_endpoint));
        }
        if (moduleController.get(AvailableModules::config) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(config_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(config_get_endpoint));
        }
        if (moduleController.get(AvailableModules::motor) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(motor_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(motor_get_endpoint));
        }
        if (moduleController.get(AvailableModules::pid) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(PID_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(PID_get_endpoint));
        }
        if (moduleController.get(AvailableModules::analogout) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(analogout_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(analogout_get_endpoint));
        }
        if (moduleController.get(AvailableModules::digitalout) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(digitalout_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(digitalout_get_endpoint));
        }
        if (moduleController.get(AvailableModules::digitalin) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(digitalin_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(digitalin_get_endpoint));
        }
        if (moduleController.get(AvailableModules::dac) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(dac_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(dac_get_endpoint));
        }
        if (moduleController.get(AvailableModules::led) != nullptr)
        {
            cJSON_AddItemToArray(doc, cJSON_CreateString(ledarr_act_endpoint));
            cJSON_AddItemToArray(doc, cJSON_CreateString(ledarr_get_endpoint));
        }
        serializeESP(doc, req);
        return ESP_OK;
    }

    esp_err_t AnalogJoystick_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::analogJoystick)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t AnalogOut_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::analogout)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t AnalogOut_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::analogout)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t Bt_startScanESP(httpd_req_t *req)
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        serializeESP(bt->scanForDevices(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t Image_getBase64ESP(httpd_req_t *req)
    {
        ImageController *img = (ImageController *)moduleController.get(AvailableModules::image);
        // we want to send the image as a base64 string over http

        //serializeESP(img->get(deserializeESP(req)), req);
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_type(req, "image/jpeg");
        httpd_resp_set_hdr(req, "Content-Encoding", "identity");
        //return httpd_resp_send(req, (const char *)favicon_32x32_png, favicon_32x32_png_len);
        //httpd_resp_send(req, img->getBase64Image().c_str(), img->getBase64Image().length());
        int buf_len = strlen(img->image_base64);
        httpd_resp_send(req, img->image_base64, buf_len);
        
        return ESP_OK;
    }

    esp_err_t Bt_connectESP(httpd_req_t *req)
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        cJSON * doc = deserializeESP(req);
        char * mac = (char*)cJSON_GetObjectItemCaseSensitive(doc,"mac")->valuestring;
        int ps = cJSON_GetObjectItemCaseSensitive(doc,"psx")->valueint;

        if (ps == 0)
        {
            bt->setMacAndConnect(mac);
        }
        else
        {
            bt->connectPsxController(mac, ps);
        }
        cJSON_Delete(doc);
        return ESP_OK;
    }
    esp_err_t Bt_getPairedDevicesESP(httpd_req_t *req)
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        serializeESP(bt->getPairedDevices(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t Bt_removeESP(httpd_req_t *req)
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        cJSON * doc = deserializeESP(req);
        char * mac = (char*)cJSON_GetObjectItemCaseSensitive(doc,"mac")->valuestring;
        bt->removePairedDevice(mac);
        cJSON_Delete(doc);
        return ESP_OK;
    }

    esp_err_t Dac_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::dac)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t Dac_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::dac)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t DigitalIn_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::digitalin)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t DigitalIn_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::digitalin)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t DigitalOut_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::digitalout)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t DigitalOut_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::digitalout)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t HomeMotor_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::home)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t HomeMotor_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::home)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t Heat_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::heat)->act(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t Heat_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::heat)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t EncoderMotor_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::encoder)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t EncoderMotor_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::encoder)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t LInearEncoderMotor_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::linearencoder)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t LinearEncoderMotor_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::linearencoder)->get(deserializeESP(req)), req);
        return ESP_OK;
    }    

    esp_err_t laser_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::laser)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t laser_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::laser)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t Led_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::led)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t led_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::led)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t FocusMotor_actESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::motor)->act(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t FocusMotor_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::motor)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t pid_setESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::pid)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t pid_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::pid)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t State_actESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::state)->act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t State_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::state)->get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t getModulesESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(), req);
        return ESP_OK;
    }

    esp_err_t scanWifiESP(httpd_req_t *req)
    {
        WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
        serializeESP(w->scan(), req);
        return ESP_OK;
    }

    esp_err_t connectToWifiESP(httpd_req_t *req)
    {
        WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
        serializeESP(w->connect(deserializeESP(req)), req);
        return ESP_OK;
    }

}