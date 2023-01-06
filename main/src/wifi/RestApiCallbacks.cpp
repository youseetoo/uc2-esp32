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

    DynamicJsonDocument deserialize()
    {
        //serializeJsonPretty(doc, Serial);
        String plain = WifiController::getServer()->arg("plain");
        DynamicJsonDocument doc(plain.length() * 8);
        deserializeJson(doc, plain);
        return doc;
    }

    void serialize(DynamicJsonDocument doc)
    {
        //serializeJsonPretty(doc, Serial);
        serializeJson(doc, output);
        WifiController::getServer()->sendHeader("Access-Control-Allow-Origin", "*", false);
        WifiController::getServer()->send_P(200, "application/json", output);
    }

    void serialize(int success)
    {
        //serializeJsonPretty(doc, Serial);
        WifiController::getServer()->sendHeader("Access-Control-Allow-Origin", "*", false);
        WifiController::getServer()->send_P(200, "application/json", output);
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
        DynamicJsonDocument doc(4096);
        doc.add(ota_endpoint);
        doc.add(update_endpoint);
        doc.add(identity_endpoint);

        doc.add(scanwifi_endpoint);
        doc.add(connectwifi_endpoint);

        if (moduleController.get(AvailableModules::laser) != nullptr)
        {
            doc.add(laser_act_endpoint);
            doc.add(laser_set_endpoint);
            doc.add(laser_get_endpoint);
        }
        if (moduleController.get(AvailableModules::config) != nullptr)
        {
            doc.add(config_act_endpoint);
            doc.add(config_set_endpoint);
            doc.add(config_get_endpoint);
        }        
        if (moduleController.get(AvailableModules::motor) != nullptr)
        {
            doc.add(motor_act_endpoint);
            doc.add(motor_set_endpoint);
            doc.add(motor_get_endpoint);
        }
        if (moduleController.get(AvailableModules::pid) != nullptr)
        {
            doc.add(PID_act_endpoint);
            doc.add(PID_set_endpoint);
            doc.add(PID_get_endpoint);
        }
        if (moduleController.get(AvailableModules::analogout) != nullptr)
        {
            doc.add(analogout_act_endpoint);
            doc.add(analogout_set_endpoint);
            doc.add(analogout_get_endpoint);
        }
        if (moduleController.get(AvailableModules::digitalout) != nullptr)
        {
            doc.add(digitalout_act_endpoint);
            doc.add(digitalout_set_endpoint);
            doc.add(digitalout_get_endpoint);
        }
        if (moduleController.get(AvailableModules::digitalin) != nullptr)
        {
            doc.add(digitalin_act_endpoint);
            doc.add(digitalin_set_endpoint);
            doc.add(digitalin_get_endpoint);
        }
        if (moduleController.get(AvailableModules::dac) != nullptr)
        {
            doc.add(dac_act_endpoint);
            doc.add(dac_set_endpoint);
            doc.add(dac_get_endpoint);
        }
        if (moduleController.get(AvailableModules::led) != nullptr)
        {
            doc.add(ledarr_act_endpoint);
            doc.add(ledarr_set_endpoint);
            doc.add(ledarr_get_endpoint);
        }
        serialize(doc);
    }

}