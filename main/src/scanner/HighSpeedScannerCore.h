/**
 * @file HighSpeedScannerCore.h
 * @brief High-speed galvo scanner core for laser projection
 * 
 * Features:
 * - Pre-blanking, imaging, flyback (cosine ease), and settling regions
 * - Configurable 3-trigger system (pixel, line, frame)
 * - Optional X-axis correction LUT for linearity
 * - Infinite loop mode when frame_count = 0
 * 
 * Hardware: ESP32-S3 + MCP4822 dual 12-bit DAC
 */

#ifndef HIGH_SPEED_SCANNER_CORE_H
#define HIGH_SPEED_SCANNER_CORE_H

#include <stdint.h>
#include <cmath>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "DAC_MCP4822.h"

// Maximum line samples (pre + imaging + flyback + settle)
constexpr int SCANNER_MAX_LINE_SAMPLES = 4096;

// X LUT size for upload (256 entries interpolated to 4096)
constexpr int SCANNER_X_LUT_N = 256;

// Maximum number of arbitrary scan points
constexpr int SCANNER_MAX_ARBITRARY_POINTS = 256;

/**
 * @brief Scan mode enumeration
 */
enum ScanMode {
    SCAN_MODE_RASTER = 0,     // Traditional raster scanning
    SCAN_MODE_ARBITRARY = 1   // Arbitrary point list scanning
};

/**
 * @brief Trigger control mode enumeration
 */
enum TriggerMode {
    TRIGGER_AUTO = 0,         // Auto: HIGH during dwell, LOW during movement
    TRIGGER_HIGH = 1,         // Force trigger HIGH (manual override)
    TRIGGER_LOW = 2,          // Force trigger LOW (manual override)
    TRIGGER_CONTINUOUS = 3    // Continuous: HIGH during entire scan
};

/**
 * @brief Single arbitrary scan point with dwell time
 */
#pragma pack(push, 1)
struct ArbitraryScanPoint {
    uint16_t x;               // X coordinate (0-4095 for 12-bit DAC)
    uint16_t y;               // Y coordinate (0-4095 for 12-bit DAC)
    uint32_t dwell_us;        // Dwell time in microseconds
    
    ArbitraryScanPoint() : x(2048), y(2048), dwell_us(1000) {}
    ArbitraryScanPoint(uint16_t x_, uint16_t y_, uint32_t dwell_us_) 
        : x(x_), y(y_), dwell_us(dwell_us_) {}
} __attribute__((packed));
#pragma pack(pop)

/**
 * @brief Nonlinear 8-bit dwell time encoding (1 µs to 1 second)
 * 
 * Uses exponential mapping: dwell_us = 10^(value / 42.5)
 * Value 0   -> ~1 µs
 * Value 127 -> ~1000 µs (1 ms)
 * Value 255 -> ~1000000 µs (1 s)
 * 
 * Useful for compact CAN transmission of dwell times.
 */
namespace DwellEncoding {
    // Encode microseconds to 8-bit nonlinear value
    inline uint8_t encode(uint32_t dwell_us) {
        if (dwell_us <= 1) return 0;
        if (dwell_us >= 1000000) return 255;
        // log10(dwell_us) * 42.5
        float log_val = log10f((float)dwell_us) * 42.5f;
        if (log_val < 0) return 0;
        if (log_val > 255) return 255;
        return (uint8_t)(log_val + 0.5f);
    }
    
    // Decode 8-bit nonlinear value back to microseconds
    inline uint32_t decode(uint8_t encoded) {
        // 10^(encoded / 42.5)
        float dwell = powf(10.0f, (float)encoded / 42.5f);
        if (dwell < 1) return 1;
        if (dwell > 1000000) return 1000000;
        return (uint32_t)(dwell + 0.5f);
    }
}

/**
 * @brief Scan configuration structure
 * 
 * All parameters for a raster scan pattern.
 * frame_count = 0 means infinite scanning until stop() is called.
 */
#pragma pack(push, 1)
struct ScanConfig {
    uint16_t nx;                    // Number of X samples per line
    uint16_t ny;                    // Number of lines (Y steps)
    uint16_t x_min, x_max;          // X range (0-4095 for 12-bit DAC)
    uint16_t y_min, y_max;          // Y range (0-4095)
    uint16_t pre_samples;           // Blanking samples before imaging
    uint16_t fly_samples;           // Flyback samples (cosine ease)
    uint16_t sample_period_us;      // Microseconds per sample (0 = max speed)
    uint16_t trig_delay_us;         // Trigger delay after position update
    uint16_t trig_width_us;         // Trigger pulse width
    uint16_t line_settle_samples;   // Settling samples after flyback
    uint8_t  enable_trigger;        // Enable pixel trigger output
    uint8_t  apply_x_lut;           // Apply X lookup table
    uint16_t frame_count;           // Number of frames (0 = infinite/continuous)
    uint8_t  bidirectional;         // Bidirectional scan: even lines min->max, odd lines max->min
    
    // Default constructor
    ScanConfig() :
        nx(256),
        ny(256),
        x_min(500),
        x_max(3500),
        y_min(500),
        y_max(3500),
        pre_samples(4),
        fly_samples(16),
        sample_period_us(1),
        trig_delay_us(0),
        trig_width_us(1),
        line_settle_samples(0),
        enable_trigger(1),
        apply_x_lut(0),
        frame_count(0),  // Default: infinite scanning
        bidirectional(0) // Default: unidirectional
    {}
} __attribute__((packed));
#pragma pack(pop)

/**
 * @brief Scanner status information
 */
struct ScannerStatus {
    bool running;
    uint16_t current_line;
    uint32_t current_frame;
    int32_t timing_overruns;
};

/**
 * @brief High-speed galvo scanner core engine
 */
class HighSpeedScannerCore {
public:
    HighSpeedScannerCore();
    ~HighSpeedScannerCore();

    /**
     * @brief Initialize scanner with DAC and trigger pins
     */
    bool init(DAC_MCP4822* dac, int trigger_pin_pixel, int trigger_pin_line, int trigger_pin_frame);

    /**
     * @brief Set scan configuration
     */
    bool setConfig(const ScanConfig& config);

    /**
     * @brief Get current scan configuration
     */
    const ScanConfig& getConfig() const { return config_; }

    /**
     * @brief Set arbitrary point list for point-based scanning
     * @param points Array of scan points
     * @param count Number of points (max SCANNER_MAX_ARBITRARY_POINTS)
     * @return true if successful, false if count exceeds maximum
     */
    bool setArbitraryPoints(const ArbitraryScanPoint* points, uint16_t count);

    /**
     * @brief Set scan mode (RASTER or ARBITRARY)
     * @param mode Scan mode
     */
    void setScanMode(ScanMode mode);

    /**
     * @brief Get current scan mode
     */
    ScanMode getScanMode() const { return scan_mode_; }

    /**
     * @brief Set trigger control mode
     * @param mode Trigger mode (AUTO, HIGH, LOW, CONTINUOUS)
     */
    void setTriggerMode(TriggerMode mode);

    /**
     * @brief Get current trigger mode
     */
    TriggerMode getTriggerMode() const { return trigger_mode_; }

    /**
     * @brief Upload X-axis correction LUT (256 entries)
     */
    void setXLUT(const uint16_t* lut256);

    /**
     * @brief Start scanning
     */
    void start();

    /**
     * @brief Stop scanning
     */
    void stop();

    /**
     * @brief Check if scanner is running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Get scanner status
     */
    ScannerStatus getStatus() const;

    /**
     * @brief Scanner task function (called by FreeRTOS)
     */
    void scannerTask();

    /**
     * @brief Create and start the scanner FreeRTOS task
     */
    bool createTask(int core_id = 1, int priority = configMAX_PRIORITIES - 1);

    /**
     * @brief stop and delete the scanner FreeRTOS task
     * 
     */
    void stopTask();
    
private:
    DAC_MCP4822* dac_ = nullptr;
    int trigger_pin_pixel_ = -1;
    int trigger_pin_line_ = -1;
    int trigger_pin_frame_ = -1;

    ScanConfig config_;
    SemaphoreHandle_t config_mutex_;

    volatile bool running_ = false;
    volatile bool task_created_ = false;
    volatile bool config_changed_ = false;  // Signal to restart scan with new config
    volatile uint32_t frame_idx_ = 0;
    volatile uint16_t line_idx_ = 0;
    volatile int32_t overruns_ = 0;
    TaskHandle_t task_handle_ = nullptr;

    // Raster scan buffers
    uint16_t line_x_[SCANNER_MAX_LINE_SAMPLES];
    uint16_t line_len_ = 0;

    // Arbitrary point scan buffers
    ArbitraryScanPoint arbitrary_points_[SCANNER_MAX_ARBITRARY_POINTS];
    uint16_t arbitrary_point_count_ = 0;
    
    // Scan mode and trigger control
    ScanMode scan_mode_ = SCAN_MODE_RASTER;
    TriggerMode trigger_mode_ = TRIGGER_AUTO;

    uint16_t x_map_[4096];
    bool x_map_valid_ = false;

    void buildLineProfile();
    uint16_t computeY(uint16_t line) const;
    uint16_t applyXMap(uint16_t x) const;
    void triggerPulsePixel();
    void triggerPulseLine();
    void triggerPulseFrame();
    void setTriggerState(bool high);  // Set trigger pin state based on mode
    
    // Arbitrary point scanning helpers
    void executeArbitraryPointScan();
    
    static inline uint16_t clamp12(int v) {
        if (v < 0) return 0;
        if (v > 4095) return 4095;
        return (uint16_t)v;
    }

    static void taskWrapper(void* param);
};

#endif // HIGH_SPEED_SCANNER_CORE_H
