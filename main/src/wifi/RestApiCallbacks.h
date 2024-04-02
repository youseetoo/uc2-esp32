#pragma once
#include "cJSON.h"
#include <PinConfig.h>

#include "esp_https_server.h"
namespace RestApi
{
    /*
        handle invalide requests with a error message
    */
    void update();
    void upload();
    /*
        load the body data from the client request into the jsondoc
    */
    cJSON *deserializeESP(httpd_req_t *req);
    /*
        fill the input from the jsondoc and send a response to the client
    */
    void serializeESP(cJSON *doc, httpd_req_t *req);
    void serializeESP(int doc, httpd_req_t *req);
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
    // TODO: void getEndpoints();
    esp_err_t getEndpoints(httpd_req_t *req);

    // TODO: void resetNvFLash();
#ifdef ANALOG_OUT_CONTROLLER
    esp_err_t AnalogOut_setESP(httpd_req_t *req);
    esp_err_t AnalogOut_getESP(httpd_req_t *req);
#endif

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
#ifdef BLUETOOTH
    esp_err_t Bt_startScanESP(httpd_req_t *req);
    esp_err_t Bt_connectESP(httpd_req_t *req);
    esp_err_t Bt_getPairedDevicesESP(httpd_req_t *req);
    esp_err_t Bt_removeESP(httpd_req_t *req);
#endif

#ifdef DAC_CONTROLLER
    esp_err_t Dac_setESP(httpd_req_t *req);
    // esp_err_t Dac_getESP(httpd_req_t *req);
#endif
#ifdef DIGITAL_IN_CONTROLLER
    esp_err_t DigitalIn_setESP(httpd_req_t *req);
    esp_err_t DigitalIn_getESP(httpd_req_t *req);
#endif
#ifdef DIGITAL_OUT_CONTROLLER
    esp_err_t DigitalOut_setESP(httpd_req_t *req);
    esp_err_t DigitalOut_getESP(httpd_req_t *req);
#endif

#ifdef HOME_MOTOR
    esp_err_t HomeMotor_setESP(httpd_req_t *req);
    esp_err_t HomeMotor_getESP(httpd_req_t *req);
#endif

#ifdef ENCODER_CONTROLLER
    esp_err_t EncoderMotor_setESP(httpd_req_t *req);
    esp_err_t EncoderMotor_getESP(httpd_req_t *req);
#endif
#ifdef LINEAR_ENCODER_CONTROLLER
    esp_err_t LinearEncoderMotor_setESP(httpd_req_t *req);
    esp_err_t LinearEncoderMotor_getESP(httpd_req_t *req);
#endif

#ifdef LASER_CONTROLLER
    esp_err_t laser_setESP(httpd_req_t *req);
    esp_err_t laser_getESP(httpd_req_t *req);
#endif

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
#ifdef LASER_CONTROLLER
    esp_err_t led_actESP(httpd_req_t *req);
    esp_err_t led_getESP(httpd_req_t *req);
#endif
#ifdef MESSAGE_CONTROLLER
    esp_err_t message_actESP(httpd_req_t *req);
    esp_err_t message_getESP(httpd_req_t *req);
#endif
#ifdef FOCUS_MOTOR
    esp_err_t FocusMotor_actESP(httpd_req_t *req);
    esp_err_t FocusMotor_getESP(httpd_req_t *req);
#endif
#ifdef PID_CONTROLLER
    esp_err_t pid_setESP(httpd_req_t *req);
    esp_err_t pid_getESP(httpd_req_t *req);
#endif

    esp_err_t State_actESP(httpd_req_t *req);
    esp_err_t State_getESP(httpd_req_t *req);

    esp_err_t getModulesESP(httpd_req_t *req);

    /*
        start a wifiscan and return the results
        endpoint:/wifi/scan
        input []
        output
        [
            "ssid1",
            "ssid2",
            ....
        ]
    */
#ifdef WIFI
    esp_err_t scanWifiESP(httpd_req_t *req);
    /*
        connect to a wifi network or create ap
        endpoint:/wifi/connect
        input
        [
            "ssid": "networkid"
            "PW" : "password"
            "AP" : 0
        ]
        output[]
    */
    esp_err_t connectToWifiESP(httpd_req_t *req);
#endif

#ifdef HEAT_CONTROLLER
    esp_err_t Heat_setESP(httpd_req_t *req);
    esp_err_t Heat_getESP(httpd_req_t *req);
    esp_err_t DS18B20_getESP(httpd_req_t *req);
    esp_err_t DS18B20_actESP(httpd_req_t *req);
#endif
};