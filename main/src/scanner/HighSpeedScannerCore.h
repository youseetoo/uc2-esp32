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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "DAC_MCP4822.h"

// Maximum line samples (pre + imaging + flyback + settle)
constexpr int SCANNER_MAX_LINE_SAMPLES = 4096;

// X LUT size for upload (256 entries interpolated to 4096)
constexpr int SCANNER_X_LUT_N = 256;

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

    uint16_t line_x_[SCANNER_MAX_LINE_SAMPLES];
    uint16_t line_len_ = 0;

    uint16_t x_map_[4096];
    bool x_map_valid_ = false;

    void buildLineProfile();
    uint16_t computeY(uint16_t line) const;
    uint16_t applyXMap(uint16_t x) const;
    void triggerPulsePixel();
    void triggerPulseLine();
    void triggerPulseFrame();
    
    static inline uint16_t clamp12(int v) {
        if (v < 0) return 0;
        if (v > 4095) return 4095;
        return (uint16_t)v;
    }

    static void taskWrapper(void* param);
};

#endif // HIGH_SPEED_SCANNER_CORE_H
