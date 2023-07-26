#pragma once
#include <ArduinoJson.h>
#include <WebServer.h>
#include <nvs_flash.h>
#include "Endpoints.h"
#include "Update.h"
#include "../config/ConfigController.h"
#include "../wifi/WifiController.h"

#include "esp_https_server.h"
namespace RestApi
{
    /*
        handle invalide requests with a error message
    */
    void handleNotFound();
    void update();
    void upload();
    /*
        load the body data from the client request into the jsondoc
    */
    DynamicJsonDocument deserialize();
    DynamicJsonDocument deserializeESP(httpd_req_t *req);
    /*
        fill the input from the jsondoc and send a response to the client
    */
    void serialize(DynamicJsonDocument doc);
    void serialize(int success);
    void serializeESP(DynamicJsonDocument doc,httpd_req_t *req);
    void serializeESP(int doc,httpd_req_t *req);
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
        "/rotator_act",
        "/rotator_set",
        "/rotator_get",
        "/ledarr_act",
        "/ledarr_set",
        "/ledarr_get"
        ]
    */
    void getEndpoints();
    
    void resetNvFLash();

    void AnalogJoystick_get();
    esp_err_t AnalogJoystick_getESP(httpd_req_t *req);

    void AnalogOut_act();
    void AnalogOut_get();
    esp_err_t AnalogOut_setESP(httpd_req_t *req);
    esp_err_t AnalogOut_getESP(httpd_req_t *req);

     /*
    returns an array that contains the visible bt devices
        endpoint:/bt_scan
        input[]
        output
        [
            {
                "name" :"HyperX",
                "mac": "01:02:03:04:05:06"
            }
            ,
            {
                "name": "",
                "mac": "01:02:03:04:05:06"
            },
        ]
    */
    void Bt_startScan();
    void Bt_connect();
    void Bt_getPairedDevices();
    void Bt_remove();

    esp_err_t Bt_startScanESP(httpd_req_t *req);
    esp_err_t Bt_connectESP(httpd_req_t *req);
    esp_err_t Bt_getPairedDevicesESP(httpd_req_t *req);
    esp_err_t Bt_removeESP(httpd_req_t *req);

    void Dac_act();
    void Dac_get();
    esp_err_t Dac_setESP(httpd_req_t *req);
    esp_err_t Dac_getESP(httpd_req_t *req);

    void DigitalIn_act();
    void DigitalIn_get();
    esp_err_t DigitalIn_setESP(httpd_req_t *req);
    esp_err_t DigitalIn_getESP(httpd_req_t *req);

    void DigitalOut_act();
    void DigitalOut_get();

    esp_err_t DigitalOut_setESP(httpd_req_t *req);
    esp_err_t DigitalOut_getESP(httpd_req_t *req);

    void HomeMotor_act();
	void HomeMotor_get();
	esp_err_t HomeMotor_setESP(httpd_req_t *req);
    esp_err_t HomeMotor_getESP(httpd_req_t *req);

    void Laser_act();
    void Laser_get();
    esp_err_t laser_setESP(httpd_req_t *req);
    esp_err_t laser_getESP(httpd_req_t *req);

        /*
        controls the leds
        endpoint:/ledarr_act
        input
        {
            "led": {
                "LEDArrMode": 1,
                "led_array": [
                    {
                        "b": 0,
                        "g": 0,
                        "id": 0,
                        "r": 0
                    }
                ]
            }
        }
        output
        []
        */
    void Led_act();
    void Led_get();
    esp_err_t Led_setESP(httpd_req_t *req);
    esp_err_t led_getESP(httpd_req_t *req);


    void FocusMotor_act();
	esp_err_t FocusMotor_actESP(httpd_req_t *req);
	void FocusMotor_get();
	esp_err_t FocusMotor_getESP(httpd_req_t *req);

    void Pid_act();
    void Pid_get();
    esp_err_t pid_setESP(httpd_req_t *req);
    esp_err_t pid_getESP(httpd_req_t *req);

    void Rotator_act();
	void Rotator_get();

	esp_err_t Rotator_actESP(httpd_req_t *req);
	esp_err_t Rotator_getESP(httpd_req_t *req);

    void State_act();
    void State_get();
	esp_err_t State_actESP(httpd_req_t *req);
	esp_err_t State_getESP(httpd_req_t *req);

    esp_err_t getModulesESP(httpd_req_t *req);
    void getModules();
};