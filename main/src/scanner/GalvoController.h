/**
 * @file GalvoController.h
 * @brief Galvo scanner controller with JSON API, CAN support, and preferences storage
 * 
 * Features:
 * - JSON API for configuration and control
 * - CAN bus integration (master sends to slave, slave executes)
 * - Preferences storage for persistent config
 * - Auto-start with saved config on boot
 * - Infinite scanning mode (frame_count = 0)
 */

#pragma once

#include "cJSON.h"
#include "HighSpeedScannerCore.h"
#include "DAC_MCP4822.h"
#include <Preferences.h>

// Preferences namespace
#define GALVO_PREFS_NAMESPACE "galvo"

/**
 * @brief GalvoData structure for CAN bus communication
 * 
 * Uses legacy field names for CAN protocol compatibility.
 * This structure is transmitted over CAN between master and slave.
 */
struct GalvoData {
    int32_t X_MIN = 500;           // Min X position (0-4095)
    int32_t X_MAX = 3500;          // Max X position (0-4095)
    int32_t Y_MIN = 500;           // Min Y position (0-4095)
    int32_t Y_MAX = 3500;          // Max Y position (0-4095)
    int32_t STEP = 256;            // Step size (used as nx/ny)
    int32_t tPixelDwelltime = 1;   // Dwell time in µs (sample_period_us)
    int32_t nFrames = 0;           // Number of frames (0=infinite)
    int32_t pre_samples = 4;       // Blanking samples before imaging
    int32_t fly_samples = 16;      // Flyback samples (cosine ease)
    int32_t line_settle_samples = 0; // Settling samples after flyback
    int32_t trig_delay_us = 0;     // Trigger delay after position update
    int32_t trig_width_us = 1;     // Trigger pulse width
    int32_t enable_trigger = 1;    // Enable pixel trigger output
    bool fastMode = false;         // Fast mode flag (unused, kept for compatibility)
    bool isRunning = false;        // Running state
    bool bidirectional = false;    // Bidirectional scan mode
    int qid = 0;                   // Queue ID
    
    // Convert to ScanConfig
    ScanConfig toScanConfig() const {
        ScanConfig cfg;
        cfg.x_min = (uint16_t)X_MIN;
        cfg.x_max = (uint16_t)X_MAX;
        cfg.y_min = (uint16_t)Y_MIN;
        cfg.y_max = (uint16_t)Y_MAX;
        cfg.nx = (uint16_t)STEP;
        cfg.ny = (uint16_t)STEP;
        cfg.sample_period_us = (uint16_t)tPixelDwelltime;
        cfg.frame_count = (uint16_t)nFrames;
        cfg.pre_samples = (uint16_t)pre_samples;
        cfg.fly_samples = (uint16_t)fly_samples;
        cfg.line_settle_samples = (uint16_t)line_settle_samples;
        cfg.trig_delay_us = (uint16_t)trig_delay_us;
        cfg.trig_width_us = (uint16_t)trig_width_us;
        cfg.enable_trigger = (uint8_t)enable_trigger;
        cfg.bidirectional = bidirectional ? 1 : 0;
        return cfg;
    }
    
    // Create from ScanConfig
    static GalvoData fromScanConfig(const ScanConfig& cfg, bool running = false) {
        GalvoData data;
        data.X_MIN = cfg.x_min;
        data.X_MAX = cfg.x_max;
        data.Y_MIN = cfg.y_min;
        data.Y_MAX = cfg.y_max;
        data.STEP = cfg.nx;
        data.tPixelDwelltime = cfg.sample_period_us;
        data.nFrames = cfg.frame_count;
        data.pre_samples = cfg.pre_samples;
        data.fly_samples = cfg.fly_samples;
        data.line_settle_samples = cfg.line_settle_samples;
        data.trig_delay_us = cfg.trig_delay_us;
        data.trig_width_us = cfg.trig_width_us;
        data.enable_trigger = cfg.enable_trigger;
        data.isRunning = running;
        data.bidirectional = (cfg.bidirectional != 0);
        return data;
    }
};

/**
 * @brief Galvo scanner controller (static class)
 * 
 * All methods are static - no instance needed.
 * On master: forwards commands via CAN
 * On slave: controls local galvo hardware
 */
class GalvoController
{
public:
    // Static API (called from main.cpp, SerialProcess, RestApi)
    static void setup();
    static void loop();
    static cJSON* get(cJSON* doc);
    static cJSON* act(cJSON* doc);
    
    // Command processing
    static cJSON* processCommand(cJSON* doc);
    static cJSON* getStatus();
    static cJSON* getConfig();
    
    // Configuration persistence
    static bool saveConfig();
    static bool loadConfig();
    
    // Direct control (for CAN receiver)
    static void start() { scanner_.start(); }
    static void stop() { scanner_.stop(); }
    static bool isRunning() { return scanner_.getStatus().running; }
    static bool setConfig(const ScanConfig& cfg) { return scanner_.setConfig(cfg); }
    static ScanConfig getCurrentConfig() { return scanner_.getConfig(); }
    
private:
    // Hardware (only used on slave)
    static DAC_MCP4822 dac_;
    static HighSpeedScannerCore scanner_;
    static Preferences prefs_;
    static bool initialized_;
    static bool auto_start_enabled_;
    static unsigned long last_status_print_;
    
    // Helpers
    static bool parseJsonConfig(cJSON* json, ScanConfig& config);
    static cJSON* configToJson(const ScanConfig& config);
};
