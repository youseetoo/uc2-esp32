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
        WifiController * w = (WifiController*)moduleController.get(AvailableModules::wifi);
        String message = "File Not Found\n\n";
        message += "URI: ";
        message += (*w->getServer()).uri();
        message += "\nMethod: ";
        message += ((*w->getServer()).method() == HTTP_GET) ? "GET" : "POST";
        message += "\nArguments: ";
        message += (*w->getServer()).args();
        message += "\n";
        for (uint8_t i = 0; i < (*w->getServer()).args(); i++)
        {
            message += " " + (*w->getServer()).argName(i) + ": " + (*w->getServer()).arg(i) + "\n";
        }
        (*w->getServer()).send(404, "text/plain", message);
    }

    DynamicJsonDocument deserialize()
    {
        //serializeJsonPretty(doc, Serial);
        WifiController * w = (WifiController*)moduleController.get(AvailableModules::wifi);
        String plain = w->getServer()->arg("plain");
        DynamicJsonDocument doc(plain.length() * 8);
        deserializeJson(doc, plain);
        return doc;
    }

    void serialize(DynamicJsonDocument doc)
    {
        //serializeJsonPretty(doc, Serial);
        Serial.println("++");
        serializeJson(doc, Serial);
        Serial.println();
        Serial.println("--");

        serializeJson(doc, output);
        WifiController * w = (WifiController*)moduleController.get(AvailableModules::wifi);
        w->getServer()->sendHeader("Access-Control-Allow-Origin", "*", false);
        w->getServer()->send_P(200, "application/json", output);
    }

    void serialize(int success)
    {
        //serializeJsonPretty(doc, Serial);
        WifiController * w = (WifiController*)moduleController.get(AvailableModules::wifi);
        w->getServer()->sendHeader("Access-Control-Allow-Origin", "*", false);
        w->getServer()->send_P(200, "application/json", output);
    }

    void update()
    {
        WifiController * w = (WifiController*)moduleController.get(AvailableModules::wifi);
        w->getServer()->sendHeader("Connection", "close");
        w->getServer()->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }

    void upload()
    {
        WifiController * w = (WifiController*)moduleController.get(AvailableModules::wifi);
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
        serialize(doc);
    }

}