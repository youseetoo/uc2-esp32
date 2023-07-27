#include "RestApiCallbacks.h"
#include "../bt/BtController.h"

#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
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

    DynamicJsonDocument deserializeESP(httpd_req_t *req)
    {
        // serializeJsonPretty(doc, Serial);
        int total_req_len = req->content_len;
        DynamicJsonDocument doc(total_req_len * 8);
        int cur_len = 0;
        char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
        int received = 0;
        if (total_req_len >= SCRATCH_BUFSIZE)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
            return doc;
        }
        while (cur_len < total_req_len)
        {
            received = httpd_req_recv(req, buf + cur_len, total_req_len);
            if (received <= 0)
            {
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
                return doc;
            }
            cur_len += received;
        }
        buf[total_req_len] = '\0';
        
        deserializeJson(doc, buf);
        return doc;
    }

    void serializeESP(DynamicJsonDocument doc,httpd_req_t *req)
    {
        serializeJson(doc, output);
        httpd_resp_set_hdr(req,"Access-Control-Allow-Origin","*");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, output);
    }

    void serializeESP(int doc,httpd_req_t *req)
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req,"Access-Control-Allow-Origin","*");
        httpd_resp_sendstr(req, output);
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
        DynamicJsonDocument doc(4096);
        doc.add(ota_endpoint);
        doc.add(update_endpoint);
        doc.add(identity_endpoint);

        doc.add(scanwifi_endpoint);
        doc.add(connectwifi_endpoint);

        if (moduleController.get(AvailableModules::laser) != nullptr)
        {
            doc.add(laser_act_endpoint);
            doc.add(laser_get_endpoint);
        }
        if (moduleController.get(AvailableModules::config) != nullptr)
        {
            doc.add(config_act_endpoint);
            doc.add(config_get_endpoint);
        }
        if (moduleController.get(AvailableModules::motor) != nullptr)
        {
            doc.add(motor_act_endpoint);
            doc.add(motor_get_endpoint);
        }
        if (moduleController.get(AvailableModules::rotator) != nullptr)
        {
            doc.add(rotator_act_endpoint);
            doc.add(rotator_get_endpoint);
        }
        if (moduleController.get(AvailableModules::pid) != nullptr)
        {
            doc.add(PID_act_endpoint);
            doc.add(PID_get_endpoint);
        }
        if (moduleController.get(AvailableModules::analogout) != nullptr)
        {
            doc.add(analogout_act_endpoint);
            doc.add(analogout_get_endpoint);
        }
        if (moduleController.get(AvailableModules::digitalout) != nullptr)
        {
            doc.add(digitalout_act_endpoint);
            doc.add(digitalout_get_endpoint);
        }
        if (moduleController.get(AvailableModules::digitalin) != nullptr)
        {
            doc.add(digitalin_act_endpoint);
            doc.add(digitalin_get_endpoint);
        }
        if (moduleController.get(AvailableModules::dac) != nullptr)
        {
            doc.add(dac_act_endpoint);
            doc.add(dac_get_endpoint);
        }
        if (moduleController.get(AvailableModules::led) != nullptr)
        {
            doc.add(ledarr_act_endpoint);
            doc.add(ledarr_get_endpoint);
        }
        serializeESP(doc,req);
        return ESP_OK;
    }

    esp_err_t AnalogJoystick_getESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(AvailableModules::analogJoystick)->get(deserializeESP(req)),req);
        return ESP_OK;
    }

	esp_err_t AnalogOut_setESP(httpd_req_t *req)
	{
		serializeESP(moduleController.get(AvailableModules::analogout)->act(deserializeESP(req)),req);
		return ESP_OK;
	}
	esp_err_t AnalogOut_getESP(httpd_req_t *req)
	{
		serializeESP(moduleController.get(AvailableModules::analogout)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

    esp_err_t Bt_startScanESP(httpd_req_t *req)
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        serializeESP(bt->scanForDevices(deserializeESP(req)), req);
        return ESP_OK;
    }

    esp_err_t Bt_connectESP(httpd_req_t *req)
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        DynamicJsonDocument doc = deserializeESP(req);
        String mac = doc["mac"];
        int ps = doc["psx"];

        if (ps == 0)
        {
            bt->setMacAndConnect(mac);
        }
        else
        {
            bt->connectPsxController(mac, ps);
        }

        doc.clear();
        serializeESP(doc, req);
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
        DynamicJsonDocument doc = deserializeESP(req);
        String mac = doc["mac"];
        bt->removePairedDevice(mac);
        doc.clear();
        serializeESP(doc, req);
        return ESP_OK;
    }

	esp_err_t Dac_setESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::dac)->act(deserializeESP(req)),req);
		return ESP_OK;
	}
    esp_err_t Dac_getESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::dac)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t DigitalIn_setESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::digitalin)->act(deserializeESP(req)),req);
		return ESP_OK;
	}
    esp_err_t DigitalIn_getESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::digitalin)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t DigitalOut_setESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::digitalout)->act(deserializeESP(req)),req);
		return ESP_OK;
	}
    esp_err_t DigitalOut_getESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::digitalout)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t HomeMotor_setESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::home)->act(deserializeESP(req)),req);
		return ESP_OK;
	}
    esp_err_t HomeMotor_getESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::home)->get(deserializeESP(req)),req);
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

	esp_err_t Led_setESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::led)->act(deserializeESP(req)),req);
		return ESP_OK;
	}
    esp_err_t led_getESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::led)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t FocusMotor_actESP(httpd_req_t *req)
	{
		serializeESP(moduleController.get(AvailableModules::motor)->act(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t FocusMotor_getESP(httpd_req_t *req)
	{
		serializeESP(moduleController.get(AvailableModules::motor)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t pid_setESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::pid)->act(deserializeESP(req)),req);
		return ESP_OK;
	}
    esp_err_t pid_getESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::pid)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t Rotator_actESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::rotator)->act(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t Rotator_getESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::rotator)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

	esp_err_t State_actESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::state)->act(deserializeESP(req)),req);
		return ESP_OK;
	}
	esp_err_t State_getESP(httpd_req_t *req){
		serializeESP(moduleController.get(AvailableModules::state)->get(deserializeESP(req)),req);
		return ESP_OK;
	}

    esp_err_t getModulesESP(httpd_req_t *req)
    {
        serializeESP(moduleController.get(),req);
        return ESP_OK;
    }

    esp_err_t scanWifiESP(httpd_req_t *req)
    {
        WifiController * w = (WifiController*)moduleController.get(AvailableModules::wifi);
        serializeESP(w->scan(),req);
        return ESP_OK;
    }

    esp_err_t connectToWifiESP(httpd_req_t *req)
	{
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		serializeESP(w->connect(deserializeESP(req)),req);
        return ESP_OK;
	}

}