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

void HighSpeedScannerCore::triggerPulsePixel(uint16_t dwell_us)
{
    if (trigger_pin_pixel_ < 0) return;
    
    // Fast GPIO register access
    GPIO.out_w1ts = (1U << trigger_pin_pixel_);
    if (dwell_us > 0) {
        esp_rom_delay_us(dwell_us);
    }
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
        4096,
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
        xSemaphoreGive(config_mutex_);
        
        if (cfg_changed) {
            SCANNER_LOG("Config changed - restarting scan");
            frame_idx_ = 0;
            line_idx_ = 0;
            overruns_ = 0;
        }

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

        const uint32_t img_start = cfg.pre_samples;
        const uint32_t img_end = cfg.pre_samples + cfg.nx;
        const bool bidir = (cfg.bidirectional != 0);

        // FRAME TRIGGER
        if (cfg.enable_trigger) {
            triggerPulseFrame();
        }

        // Scan all lines (Y steps)
        for (uint16_t ly = 0; ly < cfg.ny && running_ && !config_changed_; ++ly) {
            line_idx_ = ly;

            // LINE TRIGGER
            if (cfg.enable_trigger) {
                triggerPulseLine();
            }

            // Determine if this line is reversed (bidirectional: odd lines go backwards)
            bool reverse_line = bidir && (ly & 1);

            // Compute Y position - lock only for the computation
            xSemaphoreTake(config_mutex_, portMAX_DELAY);
            uint16_t y12 = computeY(ly);
            bool do_lut = (config_.apply_x_lut != 0) && x_map_valid_;
            bool do_trig = (config_.enable_trigger != 0);
            uint16_t sp_us = config_.sample_period_us;
            xSemaphoreGive(config_mutex_);

            // Set Y position once per line
            dac_->setY(y12);
            dac_->ldacPulse();

            int64_t next_t = esp_timer_get_time();

            // Scan one line
            for (uint32_t i = 0; i < line_len && running_ && !config_changed_; ++i) {
                next_t += sp_us;

                // Get X position - reverse index for bidirectional odd lines
                uint32_t idx = reverse_line ? (line_len - 1 - i) : i;
                uint16_t x12 = line_x_[idx];
                if (do_lut) x12 = applyXMap(x12);

                // Update X position via DAC
                dac_->setX(x12);
                dac_->ldacPulse();

                // PIXEL TRIGGER (only during imaging region)
                // For reverse lines, adjust the trigger region accordingly
                uint32_t effective_i = reverse_line ? (line_len - 1 - i) : i;
                if (do_trig && (effective_i >= img_start) && (effective_i < img_end)) {
                    if (cfg.trig_delay_us > 0) {
                        esp_rom_delay_us(cfg.trig_delay_us);
                    }
                    triggerPulsePixel(cfg.trig_width_us);
                }

                // Timing control
                if (sp_us > 0) {
                    while (true) {
                        int64_t now = esp_timer_get_time();
                        if (now >= next_t) {
                            if (now - next_t > (int64_t)sp_us) {
                                overruns_++;
                            }
                            break;
                        }
                    }
                }
                // Note: At max speed (sp_us=0), we rely on the per-line watchdog reset
                // No delay needed per-pixel since IDLE0 watchdog is disabled for galvo builds
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
    }
}
