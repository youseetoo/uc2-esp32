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
#ifdef IS_LASER
#include "../laser/LaserController.h"
#endif
#if defined IS_DAC || defined IS_DAC_FAKE
#include "../dac/DacController.h"
#endif
#ifdef IS_ANALOG
#include "../analog/AnalogController.h"
#endif
#ifdef IS_DIGITAL
#include "../digital/DigitalController.h"
#endif
#ifdef IS_PID
#include "../pid/PidController.h"
#endif
#ifdef IS_READSENSOR
#include "../sensor/SensorController.h"
#endif
#ifdef IS_SLM
#include "../slm/SlmController.h"
#endif
#if defined IS_DAC || defined IS_DAC_FAKE
#include "../dac/DacController.h"
#endif
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