/**
 * @file GalvoController.h
 * @brief Galvo scanner controller with JSON API and preferences storage
 * 
 * Features:
 * - JSON API for configuration and control
 * - Preferences storage for persistent config
 * - Auto-start with saved config on boot
 * - Infinite scanning mode (frame_count = 0)
 * - CAN bus integration
 */

#pragma once

#include "cJSON.h"
#include "HighSpeedScannerCore.h"
#include "DAC_MCP4822.h"
#include <Preferences.h>

// Preferences namespace
#define GALVO_PREFS_NAMESPACE "galvo"

/**
 * @brief Legacy GalvoData structure for CAN bus compatibility
 * 
 * Uses legacy field names for CAN protocol backward compatibility.
 * This structure is used for CAN communication between master and slave.
 */
struct GalvoData {
    // Legacy CAN field names (keep for backward compatibility)
    int32_t X_MIN = 500;           // Min X position (0-4095)
    int32_t X_MAX = 3500;          // Max X position (0-4095)
    int32_t Y_MIN = 500;           // Min Y position (0-4095)
    int32_t Y_MAX = 3500;          // Max Y position (0-4095)
    int32_t STEP = 256;            // Step size (used as nx/ny)
    int32_t tPixelDwelltime = 1;   // Dwell time in µs (sample_period_us)
    int32_t nFrames = 0;           // Number of frames (0=infinite)
    bool fastMode = false;         // Fast mode flag
    bool isRunning = false;        // Running state
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
        cfg.enable_trigger = 1;
        return cfg;
    }
    
    // Create from ScanConfig
    static GalvoData fromScanConfig(const ScanConfig& cfg, bool running = false) {
        GalvoData data;
        data.X_MIN = cfg.x_min;
        data.X_MAX = cfg.x_max;
        data.Y_MIN = cfg.y_min;
        data.Y_MAX = cfg.y_max;
        data.STEP = cfg.nx;  // Use nx as STEP
        data.tPixelDwelltime = cfg.sample_period_us;
        data.nFrames = cfg.frame_count;
        data.isRunning = running;
        return data;
    }
};

/**
 * @brief Galvo scanner controller
 * 
 * Integrates HighSpeedScannerCore with JSON API, preferences storage,
 * and optional CAN bus commands.
 */
class GalvoController
{
public:
    GalvoController() = default;
    ~GalvoController() = default;
    
    /**
     * @brief Static API for SerialProcess and RestApi compatibility
     * 
     * These static methods use the global singleton instance.
     */
    static cJSON* get(cJSON* doc);
    static cJSON* act(cJSON* doc);
    
    /**
     * @brief Get singleton instance
     */
    static GalvoController& getInstance();
    
    /**
     * @brief Static setup (call from main.cpp)
     */
    static void setup();
    
    /**
     * @brief Static loop (call from main.cpp)
     */
    static void loop();

    /**
     * @brief Process JSON command
     * 
     * JSON API:
     * - Set config and start: {"task":"/galvo_act", "config":{...}}
     * - Stop scanning: {"task":"/galvo_act", "stop":true}
     * - Save config: {"task":"/galvo_act", "save":true}
     * - Get status: {"task":"/galvo_get"}
     * 
     * Config parameters:
     * - nx, ny: scan resolution (default 256x256)
     * - x_min, x_max: X range (0-4095)
     * - y_min, y_max: Y range (0-4095)
     * - pre_samples: blanking samples before line (default 10)
     * - fly_samples: flyback samples (default 64)
     * - sample_period_us: microseconds per sample (default 1)
     * - trig_delay_us: trigger delay (default 0)
     * - trig_width_us: trigger pulse width (default 2)
     * - line_settle_samples: settle samples after flyback (default 4)
     * - enable_trigger: 1 to enable triggers (default 1)
     * - apply_x_lut: 1 to apply X lookup table (default 0)
     * - frame_count: frames to scan, 0=infinite (default 0)
     * 
     * @param doc JSON document
     * @return Response JSON (caller must free)
     */
    cJSON* processCommand(cJSON* doc);

    /**
     * @brief Get scan status as JSON
     * @return JSON object with status (caller must free)
     */
    cJSON* getStatus();

    /**
     * @brief Get current configuration as JSON
     * @return JSON object with config (caller must free)
     */
    cJSON* getConfig();

    /**
     * @brief Save current configuration to preferences
     * @return true if saved successfully
     */
    bool saveConfig();

    /**
     * @brief Load configuration from preferences
     * @return true if loaded successfully
     */
    bool loadConfig();

    /**
     * @brief Check if scanner is running
     */
    bool isRunning() const { return scanner_.getStatus().running; }

    /**
     * @brief Start scanning
     */
    void start() { scanner_.start(); }

    /**
     * @brief Stop scanning
     */
    void stop() { scanner_.stop(); }

    /**
     * @brief Get internal scanner reference (for advanced use)
     */
    HighSpeedScannerCore& getScanner() { return scanner_; }

private:
    DAC_MCP4822 dac_;
    HighSpeedScannerCore scanner_;
    Preferences prefs_;
    bool initialized_ = false;
    bool auto_start_enabled_ = false;  // Set to true to auto-start after boot
    unsigned long last_status_print_ = 0;
    
    // Internal instance methods
    bool doSetup();
    void doLoop();
    
    // Parse JSON config into ScanConfig
    bool parseJsonConfig(cJSON* json, ScanConfig& config);
    
    // Create JSON from ScanConfig
    cJSON* configToJson(const ScanConfig& config);
};
