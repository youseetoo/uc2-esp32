/**
 * @file HighSpeedScannerCore.cpp
 * @brief High-speed galvo scanner core implementation
 * 
 * Based on LASERSCANNER standalone project.
 * Uses printf for guaranteed serial output.
 */

#include "HighSpeedScannerCore.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "esp_rom_sys.h"
#include <cmath>
#include <cstring>
#include <stdio.h>

static const char* TAG = "Scanner";

// Use printf for guaranteed output
#define SCANNER_LOG(fmt, ...) printf("[%s] " fmt "\n", TAG, ##__VA_ARGS__)

HighSpeedScannerCore::HighSpeedScannerCore()
{
    config_mutex_ = xSemaphoreCreateMutex();
    buildLineProfile();
    
    // Initialize arbitrary point buffer with default center point
    arbitrary_points_[0] = ArbitraryScanPoint(2048, 2048, 1000);
    arbitrary_point_count_ = 0;  // Start with no points (disabled)
}

HighSpeedScannerCore::~HighSpeedScannerCore()
{
    stop();
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    if (config_mutex_) {
        vSemaphoreDelete(config_mutex_);
    }
}

bool HighSpeedScannerCore::init(DAC_MCP4822* dac, int trigger_pin_pixel, 
                                 int trigger_pin_line, int trigger_pin_frame)
{
    SCANNER_LOG("init() called with pins: pixel=%d, line=%d, frame=%d", 
                trigger_pin_pixel, trigger_pin_line, trigger_pin_frame);
    
    if (!dac) {
        SCANNER_LOG("ERROR: DAC pointer is null");
        return false;
    }

    dac_ = dac;
    trigger_pin_pixel_ = trigger_pin_pixel;
    trigger_pin_line_ = trigger_pin_line;
    trigger_pin_frame_ = trigger_pin_frame;

    // Configure all three trigger pins
    uint64_t pin_mask = 0;
    if (trigger_pin_pixel_ >= 0) pin_mask |= (1ULL << trigger_pin_pixel_);
    if (trigger_pin_line_ >= 0) pin_mask |= (1ULL << trigger_pin_line_);
    if (trigger_pin_frame_ >= 0) pin_mask |= (1ULL << trigger_pin_frame_);
    
    if (pin_mask != 0) {
        gpio_config_t io = {};
        io.intr_type = GPIO_INTR_DISABLE;
        io.mode = GPIO_MODE_OUTPUT;
        io.pin_bit_mask = pin_mask;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io);
        
        if (trigger_pin_pixel_ >= 0) gpio_set_level((gpio_num_t)trigger_pin_pixel_, 0);
        if (trigger_pin_line_ >= 0) gpio_set_level((gpio_num_t)trigger_pin_line_, 0);
        if (trigger_pin_frame_ >= 0) gpio_set_level((gpio_num_t)trigger_pin_frame_, 0);
        
        SCANNER_LOG("Trigger pins configured: Pixel=GPIO%d, Line=GPIO%d, Frame=GPIO%d", 
                    trigger_pin_pixel_, trigger_pin_line_, trigger_pin_frame_);
    }

    buildLineProfile();

    // Park mirrors at start position
    SCANNER_LOG("Parking mirrors at x=%d, y=%d", config_.x_min, config_.y_min);
    dac_->setX(config_.x_min);
    dac_->setY(config_.y_min);
    dac_->ldacPulse();

    SCANNER_LOG("Initialized: %dx%d, X=[%d,%d], Y=[%d,%d]",
                config_.nx, config_.ny, config_.x_min, config_.x_max, 
                config_.y_min, config_.y_max);
    return true;
}

bool HighSpeedScannerCore::setConfig(const ScanConfig& config)
{
    // Validate configuration
    uint32_t line_total = (uint32_t)config.pre_samples + config.nx + 
                          config.fly_samples + config.line_settle_samples;
    
    SCANNER_LOG("setConfig: nx=%d, ny=%d, x=[%d,%d], y=[%d,%d], period=%dus, frames=%d",
                config.nx, config.ny, config.x_min, config.x_max,
                config.y_min, config.y_max, config.sample_period_us, config.frame_count);
    
    if (config.nx == 0 || config.ny == 0) {
        SCANNER_LOG("ERROR: Invalid nx or ny (both must be > 0)");
        return false;
    }
    
    if (line_total == 0 || line_total > SCANNER_MAX_LINE_SAMPLES) {
        SCANNER_LOG("ERROR: Line total %lu out of range (max %d)", 
                    (unsigned long)line_total, SCANNER_MAX_LINE_SAMPLES);
        return false;
    }
    
    if (config.x_min >= config.x_max || config.y_min >= config.y_max) {
        SCANNER_LOG("ERROR: Invalid range");
        return false;
    }
    
    if (config.x_max > 4095 || config.y_max > 4095) {
        SCANNER_LOG("ERROR: Range exceeds 12-bit DAC limit (4095)");
        return false;
    }

    xSemaphoreTake(config_mutex_, portMAX_DELAY);
    config_ = config;
    buildLineProfile();
    config_changed_ = true;  // Signal task to restart with new config
    xSemaphoreGive(config_mutex_);

    SCANNER_LOG("Config applied successfully, line_len=%d, bidirectional=%d", line_len_, config.bidirectional);
    return true;
}

bool HighSpeedScannerCore::setArbitraryPoints(const ArbitraryScanPoint* points, uint16_t count)
{
    if (!points || count == 0 || count > SCANNER_MAX_ARBITRARY_POINTS) {
        SCANNER_LOG("ERROR: Invalid point count %d (max %d)", count, SCANNER_MAX_ARBITRARY_POINTS);
        return false;
    }

    xSemaphoreTake(config_mutex_, portMAX_DELAY);
    
    // Copy points to internal buffer
    for (uint16_t i = 0; i < count; ++i) {
        arbitrary_points_[i] = points[i];
        
        // Clamp coordinates to 12-bit range
        if (arbitrary_points_[i].x > 4095) arbitrary_points_[i].x = 4095;
        if (arbitrary_points_[i].y > 4095) arbitrary_points_[i].y = 4095;
    }
    
    arbitrary_point_count_ = count;
    config_changed_ = true;  // Signal task to restart with new config
    
    xSemaphoreGive(config_mutex_);

    SCANNER_LOG("Arbitrary points loaded: %d points", count);
    return true;
}

void HighSpeedScannerCore::setScanMode(ScanMode mode)
{
    xSemaphoreTake(config_mutex_, portMAX_DELAY);
    scan_mode_ = mode;
    config_changed_ = true;
    xSemaphoreGive(config_mutex_);
    
    SCANNER_LOG("Scan mode set to %s", (mode == SCAN_MODE_RASTER) ? "RASTER" : "ARBITRARY");
}

void HighSpeedScannerCore::setTriggerMode(TriggerMode mode)
{
    xSemaphoreTake(config_mutex_, portMAX_DELAY);
    trigger_mode_ = mode;
    xSemaphoreGive(config_mutex_);
    
    const char* mode_str = "UNKNOWN";
    switch (mode) {
        case TRIGGER_AUTO: mode_str = "AUTO"; break;
        case TRIGGER_HIGH: mode_str = "HIGH"; break;
        case TRIGGER_LOW: mode_str = "LOW"; break;
        case TRIGGER_CONTINUOUS: mode_str = "CONTINUOUS"; break;
    }
    SCANNER_LOG("Trigger mode set to %s", mode_str);
}

void HighSpeedScannerCore::setXLUT(const uint16_t* lut256)
{
    if (!lut256) return;

    for (int x = 0; x < 4096; ++x) {
        int idx = (x * (SCANNER_X_LUT_N - 1) + 2047) / 4095;
        if (idx < 0) idx = 0;
        if (idx >= SCANNER_X_LUT_N) idx = SCANNER_X_LUT_N - 1;
        x_map_[x] = lut256[idx] & 0x0FFF;
    }
    x_map_valid_ = true;
    SCANNER_LOG("X LUT updated");
}

void HighSpeedScannerCore::start()
{
    SCANNER_LOG("start() called");
    frame_idx_ = 0;
    line_idx_ = 0;
    overruns_ = 0;
    config_changed_ = false;  // Clear any pending config change
    running_ = true;
    SCANNER_LOG("Scanning STARTED (frame_count=%d, 0=infinite)", config_.frame_count);
}

void HighSpeedScannerCore::stop()
{
    SCANNER_LOG("stop() called");
    running_ = false;
    config_changed_ = false;  // Clear config change flag
    SCANNER_LOG("Scanning STOPPED");
}

ScannerStatus HighSpeedScannerCore::getStatus() const
{
    return {
        .running = running_,
        .current_line = line_idx_,
        .current_frame = frame_idx_,
        .timing_overruns = overruns_
    };
}

void HighSpeedScannerCore::buildLineProfile()
{
    uint32_t total = (uint32_t)config_.pre_samples + config_.nx + 
                     config_.fly_samples + config_.line_settle_samples;
    
    if (total == 0 || total > SCANNER_MAX_LINE_SAMPLES) {
        line_len_ = 0;
        return;
    }

    uint32_t k = 0;

    // Pre-blanking region (hold at x_min)
    for (uint32_t i = 0; i < config_.pre_samples; ++i) {
        line_x_[k++] = config_.x_min;
    }

    // Imaging region (linear ramp from x_min to x_max)
    if (config_.nx <= 1) {
        line_x_[k++] = config_.x_min;
    } else {
        int32_t dx = (int32_t)config_.x_max - (int32_t)config_.x_min;
        for (uint32_t i = 0; i < config_.nx; ++i) {
            int32_t x = (int32_t)config_.x_min + (dx * (int32_t)i) / (int32_t)(config_.nx - 1);
            line_x_[k++] = clamp12(x);
        }
    }

    // Flyback region (cosine ease from x_max to x_min)
    if (config_.fly_samples == 0) {
        // No flyback
    } else if (config_.fly_samples == 1) {
        line_x_[k++] = config_.x_min;
    } else {
        float x0 = (float)config_.x_max;
        float x1 = (float)config_.x_min;
        for (uint32_t i = 0; i < config_.fly_samples; ++i) {
            float t = (float)i / (float)(config_.fly_samples - 1);
            float s = 0.5f - 0.5f * cosf((float)M_PI * t);
            float xf = x0 + (x1 - x0) * s;
            line_x_[k++] = clamp12((int)(xf + 0.5f));
        }
    }

    // Settle region
    for (uint32_t i = 0; i < config_.line_settle_samples; ++i) {
        line_x_[k++] = config_.x_min;
    }

    line_len_ = (uint16_t)k;
}

uint16_t HighSpeedScannerCore::computeY(uint16_t line) const
{
    if (config_.ny <= 1) return config_.y_min;
    int32_t dy = (int32_t)config_.y_max - (int32_t)config_.y_min;
    int32_t y = (int32_t)config_.y_min + (dy * (int32_t)line) / (int32_t)(config_.ny - 1);
    return clamp12(y);
}

uint16_t HighSpeedScannerCore::applyXMap(uint16_t x) const
{
    if (!x_map_valid_) return x;
    return x_map_[x & 0x0FFF];
}

void HighSpeedScannerCore::triggerPulsePixel()
{
    if (trigger_pin_pixel_ < 0) return;
    
    // Fast GPIO register access
    GPIO.out_w1ts = (1U << trigger_pin_pixel_);
    GPIO.out_w1tc = (1U << trigger_pin_pixel_);
}

void HighSpeedScannerCore::triggerPulseLine()
{
    if (trigger_pin_line_ < 0) return;
    
    GPIO.out_w1ts = (1U << trigger_pin_line_);
    esp_rom_delay_us(5);
    GPIO.out_w1tc = (1U << trigger_pin_line_);
}

void HighSpeedScannerCore::triggerPulseFrame()
{
    if (trigger_pin_frame_ < 0) return;
    
    GPIO.out_w1ts = (1U << trigger_pin_frame_);
    esp_rom_delay_us(5);
    GPIO.out_w1tc = (1U << trigger_pin_frame_);
}

void HighSpeedScannerCore::setTriggerState(bool high)
{
    if (trigger_pin_pixel_ < 0) return;
    
    // Check trigger mode priority
    TriggerMode mode;
    xSemaphoreTake(config_mutex_, portMAX_DELAY);
    mode = trigger_mode_;
    xSemaphoreGive(config_mutex_);
    
    // Manual override has highest priority
    if (mode == TRIGGER_HIGH) {
        GPIO.out_w1ts = (1U << trigger_pin_pixel_);
        return;
    }
    if (mode == TRIGGER_LOW) {
        GPIO.out_w1tc = (1U << trigger_pin_pixel_);
        return;
    }
    
    // Continuous mode: always HIGH during scan
    if (mode == TRIGGER_CONTINUOUS) {
        GPIO.out_w1ts = (1U << trigger_pin_pixel_);
        return;
    }
    
    // Auto mode: respect requested state
    if (high) {
        GPIO.out_w1ts = (1U << trigger_pin_pixel_);
    } else {
        GPIO.out_w1tc = (1U << trigger_pin_pixel_);
    }
}

void HighSpeedScannerCore::taskWrapper(void* param)
{
    HighSpeedScannerCore* scanner = static_cast<HighSpeedScannerCore*>(param);
    scanner->scannerTask();
}

bool HighSpeedScannerCore::createTask(int core_id, int priority)
{
    if (task_created_) {
        SCANNER_LOG("Task already created");
        return true;
    }

    SCANNER_LOG("Creating scanner task on core %d with priority %d", core_id, priority);
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        taskWrapper,
        "galvo_scanner",
        8192,  // Increased from 4096 to prevent stack overflow with arbitrary point arrays
        this,
        priority,
        &task_handle_,
        core_id
    );

    if (ret == pdPASS) {
        task_created_ = true;
        SCANNER_LOG("Scanner task created successfully");
        return true;
    } else {
        SCANNER_LOG("ERROR: Failed to create scanner task");
        return false;
    }
}

void HighSpeedScannerCore::stopTask()
{
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
        task_created_ = false;
        SCANNER_LOG("Scanner task stopped and deleted");
    }
}

void HighSpeedScannerCore::scannerTask()
{
    SCANNER_LOG("Scanner task started on core %d", xPortGetCoreID());
    
    // Subscribe this task to the watchdog - we'll reset it periodically
    // Note: IDLE0 watchdog is disabled in sdkconfig for galvo builds
    // but we keep the scanner task watched for safety
    esp_err_t wdt_err = esp_task_wdt_add(NULL);
    if (wdt_err == ESP_OK) {
        SCANNER_LOG("Scanner task added to watchdog");
    } else {
        SCANNER_LOG("Watchdog add returned %d (may already be added or not initialized)", wdt_err);
    }

    while (true) {
        // Reset watchdog at start of each iteration
        esp_task_wdt_reset();

        if (!running_) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Check if config changed - restart frame with new config
        xSemaphoreTake(config_mutex_, portMAX_DELAY);
        bool cfg_changed = config_changed_;
        config_changed_ = false;
        ScanMode current_mode = scan_mode_;
        xSemaphoreGive(config_mutex_);
        
        if (cfg_changed) {
            SCANNER_LOG("Config changed - restarting scan");
            frame_idx_ = 0;
            line_idx_ = 0;
            overruns_ = 0;
        }

        // Route to appropriate scan implementation based on mode
        if (current_mode == SCAN_MODE_ARBITRARY) {
            executeArbitraryPointScan();
            
            // Check if we should continue looping
            frame_idx_++;
            xSemaphoreTake(config_mutex_, portMAX_DELAY);
            uint16_t fc = config_.frame_count;
            xSemaphoreGive(config_mutex_);
            
            if (fc != 0 && frame_idx_ >= fc) {
                running_ = false;
                SCANNER_LOG("Arbitrary point scan completed %d frames", fc);
            }
            
            taskYIELD();
            vTaskDelay(1);
            continue;
        }
        else if (current_mode == SCAN_MODE_RASTER)
        {       
        // RASTER SCAN MODE (existing implementation)
        // Snapshot configuration for this frame
        ScanConfig cfg;
        xSemaphoreTake(config_mutex_, portMAX_DELAY);
        cfg = config_;
        uint16_t line_len = line_len_;
        xSemaphoreGive(config_mutex_);

        if (line_len == 0 || cfg.nx == 0 || cfg.ny == 0) {
            SCANNER_LOG("Invalid configuration, stopping");
            running_ = false;
            continue;
        }

        // Log frame start (only every 100 frames to avoid spam)
        if (frame_idx_ % 100 == 0) {
            SCANNER_LOG("Frame %lu starting, bidirectional=%d", (unsigned long)frame_idx_, cfg.bidirectional);
        }

        const bool bidir = (cfg.bidirectional != 0);
        
        const uint32_t img_start = cfg.pre_samples;
        const uint32_t img_end = cfg.pre_samples + cfg.nx;

        // FRAME TRIGGER
        /*
        if (1) { // TODO : ensure it's always true ; cfg.enable_trigger) {
            triggerPulseFrame();
        }
            */

        // Scan all lines (Y steps)
        for (uint16_t ly = 0; ly < cfg.ny && running_ && !config_changed_; ++ly) {
            line_idx_ = ly;

            // LINE TRIGGER
            /*
            if (cfg.enable_trigger) {
                // triggerPulseLine();
            }
            */

            // Determine if this line is reversed (bidirectional: odd lines go backwards)
            bool reverse_line = bidir && (ly & 1);

            // Compute Y position - lock only for the computation
            xSemaphoreTake(config_mutex_, portMAX_DELAY);
            uint16_t y12 = computeY(ly);
            bool do_lut = (config_.apply_x_lut != 0) && x_map_valid_;
            bool do_trig = 1; //(config_.enable_trigger != 0);
            uint16_t sp_us = config_.sample_period_us;
            xSemaphoreGive(config_mutex_);

            // Set Y position once per line
            dac_->setY(y12);
            dac_->ldacPulse();

            int64_t next_t = esp_timer_get_time();

            // Scan one line
            for (uint32_t i = 0; i < line_len && running_ && !config_changed_; ++i) {
                next_t += sp_us;

                // TIMING CONTROL: Wait until exact time for periodic trigger
                // This ensures precise timing regardless of processing variations
                int64_t now;
                while (true) {
                    now = esp_timer_get_time();
                    if (now >= next_t) {
                        break;
                    }
                }

                // Check for timing overrun (loop too slow)
                int64_t overrun_us = now - next_t;
                if (overrun_us > (int64_t)sp_us) {
                    overruns_++;
                    // Log critical overruns (>10% of period)
                    if (overruns_ % 100 == 1) {
                        //SCANNER_LOG("Timing overrun: %lld µs (period: %d µs)", overrun_us, sp_us);
                    }
                }

                // PIXEL TRIGGER - Fire at exact periodic interval
                // Frame, Line, and first Pixel trigger happen simultaneously
                // Only during imaging region, minimal pulse width
                if (do_trig && (i >= img_start) && (i < img_end)) {
                    bool is_first_pixel = (i == img_start);
                    bool is_first_line = (ly == 0);
                    
                    // Synchronize all triggers on first pixel of first line
                    if (is_first_line && is_first_pixel) {
                        // All three triggers fire simultaneously
                        uint32_t trigger_mask = 0;
                        if (trigger_pin_frame_ >= 0) trigger_mask |= (1U << trigger_pin_frame_);
                        if (trigger_pin_line_ >= 0) trigger_mask |= (1U << trigger_pin_line_);
                        if (trigger_pin_pixel_ >= 0) trigger_mask |= (1U << trigger_pin_pixel_);
                        
                        GPIO.out_w1ts = trigger_mask;  // Set all at once
                        GPIO.out_w1tc = trigger_mask;  // Clear all at once
                    }
                    else if (is_first_pixel) {
                        // First pixel of subsequent lines: Line + Pixel trigger
                        uint32_t trigger_mask = 0;
                        if (trigger_pin_line_ >= 0) trigger_mask |= (1U << trigger_pin_line_);
                        if (trigger_pin_pixel_ >= 0) trigger_mask |= (1U << trigger_pin_pixel_);
                        
                        GPIO.out_w1ts = trigger_mask;
                        GPIO.out_w1tc = trigger_mask;
                    }
                    else {
                        // Subsequent pixels: Only pixel trigger
                        if (trigger_pin_pixel_ >= 0) {
                            GPIO.out_w1ts = (1U << trigger_pin_pixel_);
                            GPIO.out_w1tc = (1U << trigger_pin_pixel_);
                        }
                    }
                }

                // DAC UPDATE: Now we have remaining time budget for processing
                // Get X position and apply LUT if enabled
                uint32_t idx = reverse_line ? (line_len - 1 - i) : i;
                uint16_t x12 = line_x_[idx];
                if (do_lut) x12 = applyXMap(x12);

                // Update X position via DAC
                dac_->setX(x12);
                dac_->ldacPulse();
            }
            
            // Reset watchdog and yield after each line to prevent timeout
            // This is critical for long scans (e.g., 256x256 at max speed)
            esp_task_wdt_reset();
            if (sp_us == 0) {
                taskYIELD();  // Brief yield at max speed - not vTaskDelay to maintain speed
            }
        }

        // If config changed mid-frame, restart immediately
        if (config_changed_) {
            SCANNER_LOG("Config changed mid-frame, restarting");
            continue;
        }

        frame_idx_++;

        // Check frame count (0 = infinite)
        xSemaphoreTake(config_mutex_, portMAX_DELAY);
        uint16_t fc = config_.frame_count;
        xSemaphoreGive(config_mutex_);
        
        if (fc != 0 && frame_idx_ >= fc) {
            SCANNER_LOG("Completed %lu frames, stopping", (unsigned long)frame_idx_);
            running_ = false;
        }
        
        // Yield to IDLE task between frames to prevent watchdog timeout
        // This allows other tasks to run briefly without impacting scan timing
        taskYIELD();
        vTaskDelay(1); // 1 tick delay between frames to keep watchdog happy
    }
}
}

void HighSpeedScannerCore::executeArbitraryPointScan()
{
    // Snapshot point buffer - only allocate space for actual points to save stack
    xSemaphoreTake(config_mutex_, portMAX_DELAY);
    uint16_t point_count = arbitrary_point_count_;
    if (point_count > SCANNER_MAX_ARBITRARY_POINTS) point_count = SCANNER_MAX_ARBITRARY_POINTS;
    
    // Allocate only needed points on stack instead of full 256-point array
    ArbitraryScanPoint* points = (ArbitraryScanPoint*)alloca(point_count * sizeof(ArbitraryScanPoint));
    for (uint16_t i = 0; i < point_count; ++i) {
        points[i] = arbitrary_points_[i];
    }
    xSemaphoreGive(config_mutex_);

    if (point_count == 0) {
        SCANNER_LOG("No arbitrary points configured, skipping");
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    // Log start (only every 100 frames)
    if (frame_idx_ % 100 == 0) {
        SCANNER_LOG("Arbitrary point scan frame %lu: %d points", (unsigned long)frame_idx_, point_count);
    }

    // Initialize trigger state based on mode
    xSemaphoreTake(config_mutex_, portMAX_DELAY);
    TriggerMode trig_mode = trigger_mode_;
    xSemaphoreGive(config_mutex_);

    // Set initial trigger state for continuous mode
    if (trig_mode == TRIGGER_CONTINUOUS || trig_mode == TRIGGER_HIGH) {
        setTriggerState(true);
    } else {
        setTriggerState(false);
    }

    // Scan all points in sequence
    for (uint16_t pt_idx = 0; pt_idx < point_count && running_ && !config_changed_; ++pt_idx) {
        const ArbitraryScanPoint& pt = points[pt_idx];
        
        // Apply X correction LUT if enabled
        uint16_t x12 = pt.x;
        xSemaphoreTake(config_mutex_, portMAX_DELAY);
        bool do_lut = (config_.apply_x_lut != 0) && x_map_valid_;
        xSemaphoreGive(config_mutex_);
        
        if (do_lut) {
            x12 = applyXMap(x12);
        }
        
        uint16_t y12 = pt.y;
        
        // Trigger LOW during movement (unless continuous/manual override)
        if (trig_mode == TRIGGER_AUTO) {
            setTriggerState(false);
        }
        
        // Move to position
        dac_->setX(x12);
        dac_->setY(y12);
        dac_->ldacPulse();
        
        // Short settling time after movement (allow galvos to reach position)
        // Use minimal delay to not waste time
        esp_rom_delay_us(1);
        
        // Trigger HIGH during dwell (unless manual override to LOW)
        if (trig_mode == TRIGGER_AUTO || trig_mode == TRIGGER_CONTINUOUS) {
            setTriggerState(true);
        }
        
        // Dwell at position
        if (pt.dwell_us > 0) {
            // Use precise timing
            int64_t dwell_start = esp_timer_get_time();
            int64_t dwell_end = dwell_start + pt.dwell_us;
            
            while (esp_timer_get_time() < dwell_end && running_ && !config_changed_) {
                // Busy wait for precise timing
                // For very long dwells, could add periodic yields
                if (pt.dwell_us > 10000) {  // > 10ms
                    taskYIELD();
                }
            }
        }
        
        // Reset watchdog periodically
        if (pt_idx % 10 == 0) {
            esp_task_wdt_reset();
        }
    }

    // Trigger LOW at end of scan (unless continuous/manual override)
    if (trig_mode == TRIGGER_AUTO) {
        setTriggerState(false);
    }
}
