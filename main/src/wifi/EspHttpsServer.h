#pragma once
#include <WiFiUdp.h> // before lwip RE: https://github.com/espressif/arduino-esp32/issues/4405
#include <lwip/sockets.h>
#include "esp_https_server.h"

esp_err_t index_get_handler(httpd_req_t *req);
esp_err_t styles_get_handler(httpd_req_t *req);
esp_err_t jquery_get_handler(httpd_req_t *req);
esp_err_t ota_get_handler(httpd_req_t *req);
esp_err_t script_get_handler(httpd_req_t *req);
esp_err_t handle_ws_req(httpd_req_t *req);

class EspHttpsServer
{
private:
    httpd_handle_t server;
    bool run = false;
    

public:
    void start_webserver();
    void stop_webserver();
    bool running();
    void sendText(char* msg);
};