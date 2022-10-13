#include "RestApiCallbacks.h"

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

    void getIdentity()
    {
        // if(DEBUG) Serial.println("Get Identity");
        WifiController::getServer()->send(200, "application/json", state.identifier_name);
    }

    void handleNotFound()
    {
        String message = "File Not Found\n\n";
        message += "URI: ";
        message += (*WifiController::getServer()).uri();
        message += "\nMethod: ";
        message += ((*WifiController::getServer()).method() == HTTP_GET) ? "GET" : "POST";
        message += "\nArguments: ";
        message += (*WifiController::getServer()).args();
        message += "\n";
        for (uint8_t i = 0; i < (*WifiController::getServer()).args(); i++)
        {
            message += " " + (*WifiController::getServer()).argName(i) + ": " + (*WifiController::getServer()).arg(i) + "\n";
        }
        (*WifiController::getServer()).send(404, "text/plain", message);
    }

    void deserialize()
    {
        // int argcount = WifiController::getServer()->args();
        /*for (int i = 0; i < argcount; i++)
        {
            log_i( "%s", WifiController::getServer()->arg(i));
        }*/
        String body = WifiController::getServer()->arg("plain");
        if (body != "")
        {
            deserializeJson(*WifiController::getJDoc(), body);
            serializeJsonPretty((*WifiController::getJDoc()), Serial);
        }
    }

    void serialize()
    {
        serializeJsonPretty((*WifiController::getJDoc()), Serial);
        serializeJson((*WifiController::getJDoc()), output);
        WifiController::getServer()->sendHeader("Access-Control-Allow-Origin", "*", false);
        WifiController::getServer()->send_P(200, "application/json", output);
    }

    void ota()
    {
        WifiController::getServer()->sendHeader("Connection", "close");
        WifiController::getServer()->send(200, "text/html", otaindex);
    }

    void update()
    {
        WifiController::getServer()->sendHeader("Connection", "close");
        WifiController::getServer()->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }

    void upload()
    {
        HTTPUpload &upload = WifiController::getServer()->upload();
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
            /* flashing firmware to ESP*/
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
    }

    void getEndpoints()
    {
        deserialize();
        WifiController::getJDoc()->clear();
        (*WifiController::getJDoc()).add(ota_endpoint);
        (*WifiController::getJDoc()).add(update_endpoint);
        (*WifiController::getJDoc()).add(identity_endpoint);

        (*WifiController::getJDoc()).add(state_act_endpoint);
        (*WifiController::getJDoc()).add(state_set_endpoint);
        (*WifiController::getJDoc()).add(state_get_endpoint);

        (*WifiController::getJDoc()).add(scanwifi_endpoint);
        (*WifiController::getJDoc()).add(connectwifi_endpoint);

        if (moduleController.get(AvailableModules::laser) != nullptr)
        {
            (*WifiController::getJDoc()).add(laser_act_endpoint);
            (*WifiController::getJDoc()).add(laser_set_endpoint);
            (*WifiController::getJDoc()).add(laser_get_endpoint);
        }
        if (moduleController.get(AvailableModules::motor) != nullptr)
        {
            (*WifiController::getJDoc()).add(motor_act_endpoint);
            (*WifiController::getJDoc()).add(motor_set_endpoint);
            (*WifiController::getJDoc()).add(motor_get_endpoint);
        }
        if (moduleController.get(AvailableModules::pid) != nullptr)
        {
            (*WifiController::getJDoc()).add(PID_act_endpoint);
            (*WifiController::getJDoc()).add(PID_set_endpoint);
            (*WifiController::getJDoc()).add(PID_get_endpoint);
        }
        if (moduleController.get(AvailableModules::analog) != nullptr)
        {
            (*WifiController::getJDoc()).add(analog_act_endpoint);
            (*WifiController::getJDoc()).add(analog_set_endpoint);
            (*WifiController::getJDoc()).add(analog_get_endpoint);
        }
        if (moduleController.get(AvailableModules::digital) != nullptr)
        {
            (*WifiController::getJDoc()).add(digital_act_endpoint);
            (*WifiController::getJDoc()).add(digital_set_endpoint);
            (*WifiController::getJDoc()).add(digital_get_endpoint);
        }
        if (moduleController.get(AvailableModules::dac) != nullptr)
        {
            (*WifiController::getJDoc()).add(dac_act_endpoint);
            (*WifiController::getJDoc()).add(dac_set_endpoint);
            (*WifiController::getJDoc()).add(dac_get_endpoint);
        }
        if (moduleController.get(AvailableModules::slm) != nullptr)
        {
            (*WifiController::getJDoc()).add(slm_act_endpoint);
            (*WifiController::getJDoc()).add(slm_set_endpoint);
            (*WifiController::getJDoc()).add(slm_get_endpoint);
        }
        if (moduleController.get(AvailableModules::led) != nullptr)
        {
            (*WifiController::getJDoc()).add(ledarr_act_endpoint);
            (*WifiController::getJDoc()).add(ledarr_set_endpoint);
            (*WifiController::getJDoc()).add(ledarr_get_endpoint);
        }
        serialize();
    }

}