#include "PinConfig.h"
#include "RestApiCallbacks.h"
#include "Endpoints.h"
#include <nvs_flash.h>
#include "Endpoints.h"
#include "Update.h"


#ifdef ANALOG_IN_CONTROLLER
#include "../analogin/AnalogInController.h"
#endif
#ifdef ANALOG_OUT_CONTROLLER
#include "../analogout/AnalogOutController.h"
#endif
#ifdef BLUETOOTH
#include "../bt/BtController.h"
#endif
#include "../config/ConfigController.h"
#ifdef DAC_CONTROLLER
#include "../dac/DacController.h"
#endif
#ifdef DIGITAL_IN_CONTROLLER
#include "../digitalin/DigitalInController.h"
#endif
#ifdef DIGITAL_OUT_CONTROLLER
#include "../digitalout/DigitalOutController.h"
#endif
#ifdef ENCODER_CONTROLLER
#include "../encoder/EncoderController.h"
#endif
#ifdef LINEAR_ENCODER_CONTROLLER
#include "../encoder/LinearEncoderController.h"
#endif
#ifdef HOME_MOTOR
#include "../home/HomeMotor.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif
#ifdef MESSAGE_CONTROLLER
#include "../message/MessageController.h"
#endif
#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#include "../motor/MotorJsonParser.h"
#endif
#ifdef GALVO_CONTROLLER
#include "../scanner/GalvoController.h"
#endif
#ifdef PID_CONTROLLER
#include "../pid/PidController.h"
#endif
#ifdef SCANNER_CONTROLLER
#include "../scanner/ScannerController.h"
#endif
#ifdef GALVO_CONTROLLER
#include "../scanner/GalvoController.h"
#endif
#include "../state/State.h"
#ifdef WIFI
#include "../wifi/WifiController.h"
#endif
#ifdef HEAT_CONTROLLER
#include "../heat/HeatController.h"
#include "../heat/DS18b20Controller.h"
#endif

#define SCRATCH_BUFSIZE (10240)

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
        // serializeJsonPretty(doc, Serial);
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
        if (total_req_len > 0)
            doc = cJSON_Parse(buf);
        return doc;
    }

    void serializeESP(cJSON *doc, httpd_req_t *req)
    {
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_type(req, "application/json");
        char *s = cJSON_PrintUnformatted(doc);
        // log_i("send:%s", s);
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

#ifdef LASER_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(laser_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(laser_get_endpoint));
#endif
#ifdef MOTOR_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(motor_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(motor_get_endpoint));
#endif
#ifdef GALVO_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(galvo_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(galvo_get_endpoint));
#endif
#ifdef PID_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(PID_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(PID_get_endpoint));
#endif
#ifdef ANALOG_OUT_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(analogout_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(analogout_get_endpoint));
#endif
#ifdef DIGITAL_OUT_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(digitalout_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(digitalout_get_endpoint));
#endif
#ifdef DIGITAL_IN_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(digitalin_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(digitalin_get_endpoint));
#endif
#ifdef DAC_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(dac_act_endpoint));
        //cJSON_AddItemToArray(doc, cJSON_CreateString(dac_get_endpoint));
#endif
#ifdef LED_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(ledarr_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(ledarr_get_endpoint));
#endif
#ifdef MESSAGE_CONTROLLER
        cJSON_AddItemToArray(doc, cJSON_CreateString(message_act_endpoint));
        cJSON_AddItemToArray(doc, cJSON_CreateString(message_get_endpoint));
#endif
        serializeESP(doc, req);
        return ESP_OK;
    }

#ifdef ANALOG_OUT_CONTROLLER
    esp_err_t AnalogOut_setESP(httpd_req_t *req)
    {
        serializeESP(AnalogOutController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t AnalogOut_getESP(httpd_req_t *req)
    {
        serializeESP(AnalogOutController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif

#ifdef BLUETOOTH
    esp_err_t Bt_startScanESP(httpd_req_t *req)
    {

        serializeESP(BtController::scanForDevices(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t Bt_connectESP(httpd_req_t *req)
    {
        cJSON *doc = deserializeESP(req);
        char *mac = (char *)cJSON_GetObjectItemCaseSensitive(doc, "mac")->valuestring;
        int ps = cJSON_GetObjectItemCaseSensitive(doc, "psx")->valueint;

        #ifdef BTHID
            BtController::setMacAndConnect(mac);
        #endif
        #ifdef PSCONTROLLER
            BtController::connectPsxController(mac, ps);
        #endif
        cJSON_Delete(doc);
        return ESP_OK;
    }
    esp_err_t Bt_getPairedDevicesESP(httpd_req_t *req)
    {
        serializeESP(BtController::getPairedDevices(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t Bt_removeESP(httpd_req_t *req)
    {
        cJSON *doc = deserializeESP(req);
        char *mac = (char *)cJSON_GetObjectItemCaseSensitive(doc, "mac")->valuestring;
        BtController::removePairedDevice(mac);
        cJSON_Delete(doc);
        return ESP_OK;
    }
#endif

#ifdef DAC_CONTROLLER
    esp_err_t Dac_setESP(httpd_req_t *req)
    {
        serializeESP(DacController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif

#ifdef DIGITAL_IN_CONTROLLER
    esp_err_t DigitalIn_setESP(httpd_req_t *req)
    {
        serializeESP(DigitalInController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t DigitalIn_getESP(httpd_req_t *req)
    {
        serializeESP(DigitalInController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif

#ifdef DIGITAL_OUT_CONTROLLER
    esp_err_t DigitalOut_setESP(httpd_req_t *req)
    {
        serializeESP(DigitalOutController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t DigitalOut_getESP(httpd_req_t *req)
    {
        serializeESP(DigitalOutController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif

#ifdef HOME_MOTOR
    esp_err_t HomeMotor_setESP(httpd_req_t *req)
    {
        serializeESP(HomeMotor::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t HomeMotor_getESP(httpd_req_t *req)
    {
        serializeESP(HomeMotor::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
#ifdef ENCODER_CONTROLLER
    esp_err_t EncoderMotor_setESP(httpd_req_t *req)
    {
        serializeESP(EncoderController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t EncoderMotor_getESP(httpd_req_t *req)
    {
        serializeESP(EncoderController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
#ifdef LINEAR_ENCODER_CONTROLLER
    esp_err_t LinearEncoderMotor_setESP(httpd_req_t *req)
    {
        serializeESP(LinearEncoderController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t LinearEncoderMotor_getESP(httpd_req_t *req)
    {
        serializeESP(LinearEncoderController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
#ifdef LASER_CONTROLLER
    esp_err_t laser_setESP(httpd_req_t *req)
    {
        serializeESP(LaserController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t laser_getESP(httpd_req_t *req)
    {
        serializeESP(LaserController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
#ifdef LED_CONTROLLER
    esp_err_t led_actESP(httpd_req_t *req)
    {
        serializeESP(LedController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t led_getESP(httpd_req_t *req)
    {
        serializeESP(LedController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
#ifdef MESSAGE_CONTROLLER
    esp_err_t message_actESP(httpd_req_t *req)
    {
        serializeESP(MessageController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t message_getESP(httpd_req_t *req)
    {
        serializeESP(MessageController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
#ifdef MOTOR_CONTROLLER
    esp_err_t FocusMotor_actESP(httpd_req_t *req)
    {
        serializeESP(MotorJsonParser::act(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t FocusMotor_getESP(httpd_req_t *req)
    {
        serializeESP(MotorJsonParser::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
#ifdef GALVO_CONTROLLER
    esp_err_t rotator_actESP(httpd_req_t *req)
    {
        serializeESP(GalvoController::act(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t rotator_getESP(httpd_req_t *req)
    {
        serializeESP(GalvoController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
#ifdef PID_CONTROLLER
    esp_err_t pid_setESP(httpd_req_t *req)
    {
        serializeESP(PidController::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t pid_getESP(httpd_req_t *req)
    {
        serializeESP(PidController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
    esp_err_t State_actESP(httpd_req_t *req)
    {
        serializeESP(State::act(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t State_getESP(httpd_req_t *req)
    {
        serializeESP(State::get(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t getModulesESP(httpd_req_t *req)
    {
        serializeESP(State::getModules(), req);
        return ESP_OK;
    }
#ifdef WIFI
    esp_err_t scanWifiESP(httpd_req_t *req)
    {
        serializeESP(WifiController::scan(), req);
        return ESP_OK;
    }

    esp_err_t connectToWifiESP(httpd_req_t *req)
    {
        serializeESP(WifiController::connect(deserializeESP(req)), req);
        return ESP_OK;
    }

#endif

#ifdef HEAT_CONTROLLER
    esp_err_t Heat_setESP(httpd_req_t *req)
    {
        serializeESP(HeatController::act(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t Heat_getESP(httpd_req_t *req)
    {
        serializeESP(HeatController::get(deserializeESP(req)), req);
        return ESP_OK;
    }
    esp_err_t DS18B20_actESP(httpd_req_t *req)
    {
        serializeESP(DS18b20Controller::act(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t DS18B20_getESP(httpd_req_t *req)
    {
        serializeESP(DS18b20Controller::get(deserializeESP(req)), req);
        return ESP_OK;
    }
#endif
}