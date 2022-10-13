#pragma once
#include "../../config.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <nvs_flash.h>
#include "Endpoints.h"
#include "parameters_wifi.h"
#include "../config/ConfigController.h"
#include "../wifi/WifiController.h"
#include "../bt/BtController.h"
#include <esp_log.h>
#include "../dac/DacController.h"
#include "../analog/AnalogController.h"
#include "../digital/DigitalController.h"
namespace RestApi
{
    /*
        handle invalide requests with a error message
    */
    void handleNotFound();
    void ota();
    void update();
    void upload();
    /*
        load the body data from the client request into the jsondoc
    */
    void deserialize();
    /*
        fill the output from the jsondoc and send a response to the client
    */
    void serialize();
    void getIdentity();
    /*
        returns an array that contains the endpoints
        endpoint:/features_get or /
        input[]
        output
        [
        "/ota",
        "/update",
        "/identity",
        "/config_act",
        "/config_set",
        "/config_get",
        "/state_act",
        "/state_set",
        "/state_get",
        "/wifi/scan",
        "/wifi/connect",
        "/motor_act",
        "/motor_set",
        "/motor_get",
        "/ledarr_act",
        "/ledarr_set",
        "/ledarr_get"
        ]
    */
    void getEndpoints();
    
    void resetNvFLash();
};