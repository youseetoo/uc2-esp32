/**
 * @file GalvoController.cpp
 * @brief Galvo scanner controller implementation
 *
 * Static class - no instance/singleton pattern needed.
 * Master mode: forwards commands via CAN
 * Slave mode: controls local galvo hardware
 */

#include "GalvoController.h"
#include "esp_log.h"
#include <cstring>
#include <stdio.h>
#include "PinConfig.h"

#ifdef CAN_BUS_ENABLED
#include "../can/can_controller.h"
#endif

static const char *TAG = "GalvoCtrl";

// Use printf for guaranteed output
#define GALVO_LOG(fmt, ...) printf("[%s] " fmt "\n", TAG, ##__VA_ARGS__)

// Static member definitions
DAC_MCP4822 GalvoController::dac_;
HighSpeedScannerCore GalvoController::scanner_;
Preferences GalvoController::prefs_;
bool GalvoController::initialized_ = false;
bool GalvoController::auto_start_enabled_ = true; // Enable auto-start by default
unsigned long GalvoController::last_status_print_ = 0;

void GalvoController::setup()
{
 
#if defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_GALVO)
    // Host/Master mode - only forward commands via CAN, no local hardware
    initialized_ = true;
    return;
#else
    delay(2000); // Wait for CAN bus to stabilize
    // Satellite/Slave mode - initialize actual hardware
    GALVO_LOG("Running in SLAVE mode (local hardware)");

    // Get pin configuration from global pinConfig
    int sdi_pin = pinConfig.galvo_sdi;
    int sck_pin = pinConfig.galvo_sck;
    int cs_pin = pinConfig.galvo_cs;
    int ldac_pin = pinConfig.galvo_ldac;
    int trig_pixel = pinConfig.galvo_trig_pixel;
    int trig_line = pinConfig.galvo_trig_line;
    int trig_frame = pinConfig.galvo_trig_frame;

    GALVO_LOG("Pin configuration:");
    GALVO_LOG("  SPI: SDI=%d, SCK=%d, CS=%d, LDAC=%d",
              sdi_pin, sck_pin, cs_pin, ldac_pin);
    GALVO_LOG("  Triggers: Pixel=%d, Line=%d, Frame=%d",
              trig_pixel, trig_line, trig_frame);

    // Initialize DAC
    GALVO_LOG("Initializing MCP4822 DAC...");
    if (!dac_.init(sdi_pin, sck_pin, cs_pin, ldac_pin))
    {
        GALVO_LOG("ERROR: DAC initialization failed!");
        return;
    }
    GALVO_LOG("DAC initialized successfully");

    // Test DAC output - park at center
    GALVO_LOG("Testing DAC - parking at center (2048, 2048)");
    dac_.setX(2048);
    dac_.setY(2048);
    dac_.ldacPulse();

    // Initialize scanner
    GALVO_LOG("Initializing scanner core...");
    if (!scanner_.init(&dac_, trig_pixel, trig_line, trig_frame))
    {
        GALVO_LOG("ERROR: Scanner initialization failed!");
        return;
    }
    GALVO_LOG("Scanner core initialized");

    // Create scanner task on Core 0 (not Core 1 where main loop runs!)
    // This prevents the watchdog issue - main loop runs on Core 1
    GALVO_LOG("Creating scanner task on Core 0...");
    if (!scanner_.createTask(0, configMAX_PRIORITIES - 2))
    {
        GALVO_LOG("ERROR: Failed to create scanner task!");
        return;
    }
    GALVO_LOG("Scanner task created on Core 0");

    // Load saved configuration from preferences
    GALVO_LOG("Loading configuration from preferences...");
    if (loadConfig())
    {
        GALVO_LOG("Configuration loaded from preferences");
    }
    else
    {
        GALVO_LOG("No saved config - using defaults");
        ScanConfig cfg; // Uses default values
        scanner_.setConfig(cfg);
    }

    // Check auto-start preference
    prefs_.begin(GALVO_PREFS_NAMESPACE, true);
    auto_start_enabled_ = prefs_.getBool("auto_start", true); // Default true
    prefs_.end();

#endif
    initialized_ = true;

    // Auto-start scanning if enabled
    if (auto_start_enabled_)
    {
        GALVO_LOG("Auto-start enabled - starting scanner...");
        scanner_.start();
    }
    else
    {
        GALVO_LOG("Auto-start disabled - scanner idle");
    }

    GALVO_LOG("====================================");
    GALVO_LOG("GalvoController setup complete!");
    GALVO_LOG("====================================");
}

void GalvoController::loop()
{
    if (!initialized_)
        return;

#if defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_GALVO)
    // Master mode - nothing to do in loop
    return;
#endif

    // Print status every 5 seconds when running
    unsigned long now = millis();
    if (now - last_status_print_ >= 5000)
    {
        last_status_print_ = now;

        ScannerStatus status = scanner_.getStatus();
        if (status.running)
        {
            GALVO_LOG("Status: frame=%lu, line=%d, overruns=%lu",
                      (unsigned long)status.current_frame,
                      status.current_line,
                      (unsigned long)status.timing_overruns);
        }
    }
}

cJSON *GalvoController::get(cJSON *doc)
{
    return getStatus();
}

cJSON *GalvoController::act(cJSON *doc)
{/*
    {"task": "/galvo_act", "config": {"nx": 256, "ny": 256, "x_min": 500, "x_max": 3500, "y_min": 500, "y_max": 3500, "sample_period_us": 1000, "frame_count": 10, "bidirectional": false}}
    full:
    {"task":"/state_get"}
    {"task": "/galvo_act", "config": {"nx":256,"ny":256,"x_min":500,"x_max":3500,"y_min":500,"y_max":3500,"pre_samples":0,"fly_samples":0,"sample_period_us":0,"enable_trigger":true,"apply_x_lut":0,"frame_count":0,"bidirectional":false}}
    {"task": "/galvo_act", "config": {"nx":256,"ny":256,"x_min":500,"x_max":3500,"y_min":500,"y_max":3500,"pre_samples":0,"fly_samples":0,"sample_period_us":0,"trig_delay_us":0,"trig_width_us":20,"line_settle_samples":0,"enable_trigger":1,"apply_x_lut":0,"frame_count":0,"bidirectional":false}}
    {"task": "/galvo_act", "config": {"nx":100,"ny":100,"x_min":500,"x_max":1500,"y_min":500,"y_max":1500,"pre_samples":16,"fly_samples":16,"sample_period_us":20,"trig_delay_us":0,"trig_width_us":20,"line_settle_samples":0,"enable_trigger":1,"apply_x_lut":0,"frame_count":0,"bidirectional":true}}

    */
    return processCommand(doc);
}

cJSON *GalvoController::processCommand(cJSON *doc)
{
    
    cJSON *response = cJSON_CreateObject();

#if defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_GALVO)
    // ========== MASTER MODE - Forward commands via CAN ==========
    // GALVO_LOG("Master mode - forwarding via CAN");

    // Check for stop command
    cJSON *stop_cmd = cJSON_GetObjectItem(doc, "stop");
    if (stop_cmd && cJSON_IsTrue(stop_cmd))
    {
        GALVO_LOG("Forwarding STOP via CAN");
        GalvoData galvoData;
        galvoData.isRunning = false;
        galvoData.nFrames = 0;
#ifdef CAN_BUS_ENABLED
        can_controller::sendGalvoDataToCANDriver(galvoData);
#endif
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Stop sent via CAN");
        return response;
    }

    // Extract config and create GalvoData for CAN transmission
    cJSON *config_obj = cJSON_GetObjectItem(doc, "config");
    if (config_obj)
    {
        GalvoData galvoData;

        cJSON *item;
        if ((item = cJSON_GetObjectItem(config_obj, "x_min")))
            galvoData.X_MIN = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "x_max")))
            galvoData.X_MAX = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "y_min")))
            galvoData.Y_MIN = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "y_max")))
            galvoData.Y_MAX = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "nx")))
            galvoData.STEP = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "sample_period_us")))
            galvoData.tPixelDwelltime = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "frame_count")))
            galvoData.nFrames = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "bidirectional")))
            galvoData.bidirectional = cJSON_IsTrue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "pre_samples")))
            galvoData.pre_samples = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "fly_samples")))
            galvoData.fly_samples = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "line_settle_samples")))
            galvoData.line_settle_samples = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "trig_delay_us")))
            galvoData.trig_delay_us = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "trig_width_us")))
            galvoData.trig_width_us = (int32_t)cJSON_GetNumberValue(item);
        if ((item = cJSON_GetObjectItem(config_obj, "enable_trigger")))
            galvoData.enable_trigger = cJSON_IsTrue(item) ? 1 : 0;

        // Get qid from doc
        if ((item = cJSON_GetObjectItem(doc, "qid")))
            galvoData.qid = (int)cJSON_GetNumberValue(item);

        galvoData.isRunning = true; // Start scanning

        // Log all parameters being sent
        if (0){
        GALVO_LOG("Sending config via CAN: nx=%d, ny=%d, x=[%d,%d], y=[%d,%d]",
                  galvoData.STEP, galvoData.STEP, galvoData.X_MIN, galvoData.X_MAX, 
                  galvoData.Y_MIN, galvoData.Y_MAX);
        GALVO_LOG("  timing: period=%dµs, pre=%d, fly=%d, settle=%d",
                  galvoData.tPixelDwelltime, galvoData.pre_samples, 
                  galvoData.fly_samples, galvoData.line_settle_samples);
        GALVO_LOG("  trigger: enable=%d, delay=%dµs, width=%dµs",
                  galvoData.enable_trigger, galvoData.trig_delay_us, galvoData.trig_width_us);
        GALVO_LOG("  frames=%d, bidir=%d, isRunning=%d",
                  galvoData.nFrames, galvoData.bidirectional, galvoData.isRunning);
}

        // Send via CAN
#ifdef CAN_BUS_ENABLED
        can_controller::sendGalvoDataToCANDriver(galvoData);
#endif

        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Config sent via CAN");
        return response;
    }

    cJSON_AddBoolToObject(response, "success", false);
    cJSON_AddStringToObject(response, "error", "Master mode - provide config to forward");
    return response;

#else
    // ========== SLAVE MODE - Execute commands locally ==========
    GALVO_LOG("Slave mode - processing command");

    // Check for stop command
    cJSON *stop_cmd = cJSON_GetObjectItem(doc, "stop");
    if (stop_cmd && cJSON_IsTrue(stop_cmd))
    {
        GALVO_LOG("STOP command received");
        scanner_.stop();
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Scanning stopped");
        return response;
    }

    // Check for save command
    cJSON *save_cmd = cJSON_GetObjectItem(doc, "save");
    if (save_cmd && cJSON_IsTrue(save_cmd))
    {
        GALVO_LOG("SAVE command received");
        bool saved = saveConfig();
        cJSON_AddBoolToObject(response, "success", saved);
        cJSON_AddStringToObject(response, "message", saved ? "Config saved" : "Save failed");
        return response;
    }

    // Check for auto_start setting
    cJSON *auto_start_cmd = cJSON_GetObjectItem(doc, "auto_start");
    if (auto_start_cmd)
    {
        bool enable = cJSON_IsTrue(auto_start_cmd);
        GALVO_LOG("AUTO_START set to %s", enable ? "true" : "false");

        prefs_.begin(GALVO_PREFS_NAMESPACE, false);
        prefs_.putBool("auto_start", enable);
        prefs_.end();

        auto_start_enabled_ = enable;
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", enable ? "Auto-start enabled" : "Auto-start disabled");
        return response;
    }

    // Check for config object (from JSON API)
    cJSON *config_obj = cJSON_GetObjectItem(doc, "config");
    if (config_obj)
    {
        GALVO_LOG("CONFIG command received");

        ScanConfig config = scanner_.getConfig(); // Start with current config

        // in case of bidreciontal scanning, we don't need pre_samples and fly_samples for timing
        if (config.bidirectional) {
            // log
            GALVO_LOG("Bidirectional scan: ignoring pre_samples and fly_samples for timing");
            config.pre_samples = 0;
            config.fly_samples = 0;
        }
        
        if (parseJsonConfig(config_obj, config))
        {
            if (scanner_.setConfig(config))
            {
                GALVO_LOG("Config applied, starting scanner...");
                scanner_.start();

                cJSON_AddBoolToObject(response, "success", true);
                cJSON_AddStringToObject(response, "message", "Config applied, scanning started");
                cJSON_AddItemToObject(response, "config", configToJson(config));
            }
            else
            {
                GALVO_LOG("ERROR: Invalid configuration");
                cJSON_AddBoolToObject(response, "success", false);
                cJSON_AddStringToObject(response, "error", "Invalid configuration values");
            }
        }
        else
        {
            GALVO_LOG("ERROR: Failed to parse config JSON");
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "error", "Failed to parse config");
        }
        return response;
    }


    // Check for X LUT
    cJSON *x_lut = cJSON_GetObjectItem(doc, "x_lut");
    if (x_lut && cJSON_IsArray(x_lut))
    {
        int size = cJSON_GetArraySize(x_lut);
        GALVO_LOG("X_LUT received with %d entries", size);

        if (size == SCANNER_X_LUT_N)
        {
            uint16_t lut[SCANNER_X_LUT_N];
            for (int i = 0; i < size; ++i)
            {
                cJSON *item = cJSON_GetArrayItem(x_lut, i);
                lut[i] = (uint16_t)cJSON_GetNumberValue(item);
            }
            scanner_.setXLUT(lut);
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddStringToObject(response, "message", "X LUT applied");
        }
        else
        {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddStringToObject(response, "error", "X LUT must have 256 entries");
        }
        return response;
    }

    // Default: return status
    GALVO_LOG("No specific command, returning status");
    cJSON_Delete(response);
    return getStatus();
#endif // Master/Slave mode selection
}

cJSON *GalvoController::getStatus()
{
    cJSON *status = cJSON_CreateObject();

#if defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_GALVO)
    // Master mode
    cJSON_AddBoolToObject(status, "master_mode", true);
    cJSON_AddBoolToObject(status, "initialized", initialized_);
    return status;
#endif

    ScannerStatus s = scanner_.getStatus();
    cJSON_AddBoolToObject(status, "running", s.running);
    cJSON_AddNumberToObject(status, "current_frame", s.current_frame);
    cJSON_AddNumberToObject(status, "current_line", s.current_line);
    cJSON_AddNumberToObject(status, "timing_overruns", s.timing_overruns);
    cJSON_AddBoolToObject(status, "auto_start", auto_start_enabled_);
    cJSON_AddBoolToObject(status, "initialized", initialized_);

    // Add current config
    cJSON_AddItemToObject(status, "config", configToJson(scanner_.getConfig()));

    return status;
}

cJSON *GalvoController::getConfig()
{
    return configToJson(scanner_.getConfig());
}

bool GalvoController::saveConfig()
{
    GALVO_LOG("Saving configuration to preferences...");

    ScanConfig cfg = scanner_.getConfig();

    prefs_.begin(GALVO_PREFS_NAMESPACE, false);

    prefs_.putUShort("nx", cfg.nx);
    prefs_.putUShort("ny", cfg.ny);
    prefs_.putUShort("x_min", cfg.x_min);
    prefs_.putUShort("x_max", cfg.x_max);
    prefs_.putUShort("y_min", cfg.y_min);
    prefs_.putUShort("y_max", cfg.y_max);
    prefs_.putUShort("pre_samp", cfg.pre_samples);
    prefs_.putUShort("fly_samp", cfg.fly_samples);
    prefs_.putUShort("period_us", cfg.sample_period_us);
    prefs_.putUShort("trig_del", cfg.trig_delay_us);
    prefs_.putUShort("trig_wid", cfg.trig_width_us);
    prefs_.putUShort("settle", cfg.line_settle_samples);
    prefs_.putUChar("en_trig", cfg.enable_trigger);
    prefs_.putUChar("x_lut", cfg.apply_x_lut);
    prefs_.putUShort("frames", cfg.frame_count);
    prefs_.putBool("saved", true);

    prefs_.end();

    GALVO_LOG("Configuration saved to preferences");
    return true;
}

bool GalvoController::loadConfig()
{
    GALVO_LOG("Loading configuration from preferences...");

    prefs_.begin(GALVO_PREFS_NAMESPACE, true);

    bool has_saved = prefs_.getBool("saved", false);
    if (!has_saved)
    {
        prefs_.end();
        GALVO_LOG("No saved configuration found");
        return false;
    }

    ScanConfig cfg;
    cfg.nx = prefs_.getUShort("nx", 256);
    cfg.ny = prefs_.getUShort("ny", 256);
    cfg.x_min = prefs_.getUShort("x_min", 500);
    cfg.x_max = prefs_.getUShort("x_max", 3500);
    cfg.y_min = prefs_.getUShort("y_min", 500);
    cfg.y_max = prefs_.getUShort("y_max", 3500);
    cfg.pre_samples = prefs_.getUShort("pre_samp", 10);
    cfg.fly_samples = prefs_.getUShort("fly_samp", 64);
    cfg.sample_period_us = prefs_.getUShort("period_us", 1);
    cfg.trig_delay_us = prefs_.getUShort("trig_del", 0);
    cfg.trig_width_us = prefs_.getUShort("trig_wid", 2);
    cfg.line_settle_samples = prefs_.getUShort("settle", 4);
    cfg.enable_trigger = prefs_.getUChar("en_trig", 1);
    cfg.apply_x_lut = prefs_.getUChar("x_lut", 0);
    cfg.frame_count = prefs_.getUShort("frames", 0);
    cfg.bidirectional = prefs_.getBool("bidirectional", false) ? 1 : 0;

    prefs_.end();

    GALVO_LOG("Loaded config: nx=%d ny=%d x=[%d,%d] y=[%d,%d] frames=%d",
              cfg.nx, cfg.ny, cfg.x_min, cfg.x_max, cfg.y_min, cfg.y_max, cfg.frame_count);

    return scanner_.setConfig(cfg);
}

bool GalvoController::parseJsonConfig(cJSON *json, ScanConfig &cfg)
{
    if (!json)
        return false;

    cJSON *item;

    if ((item = cJSON_GetObjectItem(json, "nx")))
        cfg.nx = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "ny")))
        cfg.ny = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "x_min")))
        cfg.x_min = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "x_max")))
        cfg.x_max = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "y_min")))
        cfg.y_min = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "y_max")))
        cfg.y_max = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "pre_samples")))
        cfg.pre_samples = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "fly_samples")))
        cfg.fly_samples = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "sample_period_us")))
        cfg.sample_period_us = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "trig_delay_us")))
        cfg.trig_delay_us = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "trig_width_us")))
        cfg.trig_width_us = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "line_settle_samples")))
        cfg.line_settle_samples = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "enable_trigger")))
        cfg.enable_trigger = (uint8_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "apply_x_lut")))
        cfg.apply_x_lut = (uint8_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "frame_count")))
        cfg.frame_count = (uint16_t)cJSON_GetNumberValue(item);
    if ((item = cJSON_GetObjectItem(json, "bidirectional")))
        cfg.bidirectional = cJSON_IsTrue(item) ? 1 : 0;

    GALVO_LOG("Parsed config: nx=%d ny=%d x=[%d,%d] y=[%d,%d] frames=%d, bidirectional=%d",
              cfg.nx, cfg.ny, cfg.x_min, cfg.x_max, cfg.y_min, cfg.y_max, cfg.frame_count, cfg.bidirectional);

    return true;
}

cJSON *GalvoController::configToJson(const ScanConfig &cfg)
{
    cJSON *json = cJSON_CreateObject();

    cJSON_AddNumberToObject(json, "nx", cfg.nx);
    cJSON_AddNumberToObject(json, "ny", cfg.ny);
    cJSON_AddNumberToObject(json, "x_min", cfg.x_min);
    cJSON_AddNumberToObject(json, "x_max", cfg.x_max);
    cJSON_AddNumberToObject(json, "y_min", cfg.y_min);
    cJSON_AddNumberToObject(json, "y_max", cfg.y_max);
    cJSON_AddNumberToObject(json, "pre_samples", cfg.pre_samples);
    cJSON_AddNumberToObject(json, "fly_samples", cfg.fly_samples);
    cJSON_AddNumberToObject(json, "sample_period_us", cfg.sample_period_us);
    cJSON_AddNumberToObject(json, "trig_delay_us", cfg.trig_delay_us);
    cJSON_AddNumberToObject(json, "trig_width_us", cfg.trig_width_us);
    cJSON_AddNumberToObject(json, "line_settle_samples", cfg.line_settle_samples);
    cJSON_AddNumberToObject(json, "enable_trigger", cfg.enable_trigger);
    cJSON_AddNumberToObject(json, "apply_x_lut", cfg.apply_x_lut);
    cJSON_AddNumberToObject(json, "frame_count", cfg.frame_count);
    cJSON_AddNumberToObject(json, "bidirectional", cfg.bidirectional);
    return json;
}
