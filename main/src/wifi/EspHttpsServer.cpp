#include "EspHttpsServer.h"
#include "../../ModuleController.h"
#include "Endpoints.h"
#include "RestApiCallbacks.h"

extern const char index_start[] asm("_binary_index_html_start");
extern const char index_end[] asm("_binary_index_html_end");
extern const char styles_start[] asm("_binary_styles_css_start");
extern const char styles_end[] asm("_binary_styles_css_end");
extern const char jquery_start[] asm("_binary_jquery_3_6_1_min_js_start");
extern const char jquery_end[] asm("_binary_jquery_3_6_1_min_js_end");
extern const char ota_start[] asm("_binary_ota_html_start");
extern const char ota_end[] asm("_binary_ota_html_end");
extern const char script_start[] asm("_binary_script_js_start");
extern const char script_end[] asm("_binary_script_js_end");
extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
extern const unsigned char servercert_end[] asm("_binary_servercert_pem_end");
extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");

// HTTP GET Handler
esp_err_t index_get_handler(httpd_req_t *req)
{
    log_i("Serve index");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_start, index_end - index_start);

    return ESP_OK;
}

esp_err_t styles_get_handler(httpd_req_t *req)
{
    log_i("Serve styles");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, styles_start, styles_end - styles_start);

    return ESP_OK;
}

esp_err_t jquery_get_handler(httpd_req_t *req)
{
    log_i("Serve jquery");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, jquery_start, jquery_end - jquery_start);

    return ESP_OK;
}

esp_err_t ota_get_handler(httpd_req_t *req)
{
    log_i("Serve ota");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, ota_start, ota_end - ota_start);

    return ESP_OK;
}

esp_err_t script_get_handler(httpd_req_t *req)
{
    log_i("Serve script");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, script_start, script_end - script_start);

    return ESP_OK;
}

void EspHttpsServer::start_webserver()
{

    // Start the httpd server
    log_i("Starting server");
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.max_uri_handlers = 25;
    conf.transport_mode = HTTPD_SSL_TRANSPORT_INSECURE;
    conf.cacert_pem  = servercert_start;
    conf.cacert_len = servercert_end - servercert_start;

    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret)
    {
        log_i("Error starting server!");
        return;
    }

    // Set URI handlers
    log_i("Registering URI handlers");
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler};
    httpd_register_uri_handler(server, &root);

    httpd_uri_t styles = {
        .uri = "/styles.css",
        .method = HTTP_GET,
        .handler = styles_get_handler};
    httpd_register_uri_handler(server, &styles);

    httpd_uri_t jquery = {
        .uri = "/jquery-3.6.1.min.js",
        .method = HTTP_GET,
        .handler = jquery_get_handler};
    httpd_register_uri_handler(server, &jquery);

    httpd_uri_t otahtml = {
        .uri = "/ota.html",
        .method = HTTP_GET,
        .handler = ota_get_handler};
    httpd_register_uri_handler(server, &otahtml);

    httpd_uri_t scriptjs = {
        .uri = "/script.js",
        .method = HTTP_GET,
        .handler = script_get_handler};
    httpd_register_uri_handler(server, &scriptjs);

    if (moduleController.get(AvailableModules::motor) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = motor_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::FocusMotor_actESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = motor_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::FocusMotor_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::rotator) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = rotator_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::Rotator_actESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = rotator_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::Rotator_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::state) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = state_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::State_actESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = state_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::State_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::dac) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = dac_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::Dac_setESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = dac_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::Dac_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::laser) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = laser_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::laser_setESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = laser_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::laser_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::analogout) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = analogout_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::AnalogOut_setESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = analogout_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::AnalogOut_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::digitalin) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = digitalin_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::DigitalIn_setESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = digitalin_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::DigitalIn_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::digitalout) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = digitalout_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::DigitalOut_setESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = digitalout_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::DigitalOut_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::pid) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = PID_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::pid_setESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = PID_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::pid_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::led) != nullptr)
    {
        httpd_uri_t motor_act = {
            .uri = ledarr_act_endpoint,
            .method = HTTP_POST,
            .handler = RestApi::Led_setESP};
        httpd_register_uri_handler(server, &motor_act);
        httpd_uri_t motor_get = {
            .uri = ledarr_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::led_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    if (moduleController.get(AvailableModules::analogJoystick) != nullptr)
    {
        httpd_uri_t motor_get = {
            .uri = analog_joystick_get_endpoint,
            .method = HTTP_GET,
            .handler = RestApi::AnalogJoystick_getESP};
        httpd_register_uri_handler(server, &motor_get);
    }
    run = true;
}

void EspHttpsServer::stop_webserver()
{
    // Stop the httpd server
    httpd_ssl_stop(server);
    run = false;
}

bool EspHttpsServer::running() { return run; }
