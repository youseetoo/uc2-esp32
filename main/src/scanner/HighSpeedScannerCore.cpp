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
#include "soc/soc_caps.h"
// Hardware pixel clock needs RMT finite TX loop count (ESP32-S3; not classic
// ESP32). IDF >= 5 provides driver/rmt_tx.h; the IDF 4.4-based Arduino core
// only has the legacy driver/rmt.h, which exposes the same loop-count feature
// via rmt_set_tx_loop_count() + autostop.
#if SOC_RMT_SUPPORT_TX_LOOP_COUNT
#if __has_include("driver/rmt_tx.h")
#define GALVO_RMT_NEW_API 1
#include "driver/rmt_tx.h"
#elif __has_include("driver/rmt.h")
#define GALVO_RMT_LEGACY_API 1
#include "driver/rmt.h"
// Use the last TX-capable channel to avoid colliding with libraries that
// allocate from 0 upwards (e.g. NeoPixel)
#define GALVO_RMT_CHANNEL RMT_CHANNEL_3
#endif
#endif
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
    disableHwPixelClock();
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    if (config_mutex_) {
        vSemaphoreDelete(config_mutex_);
    }
}

bool HighSpeedScannerCore::init(DAC_MCP4822* dac, int trigger_pin_pixel,
                                 int trigger_pin_line, int trigger_pin_frame,
                                 int laser_pin)
{
    SCANNER_LOG("init() called with pins: pixel=%d, line=%d, frame=%d, laser=%d",
                trigger_pin_pixel, trigger_pin_line, trigger_pin_frame, laser_pin);

    if (!dac) {
        SCANNER_LOG("ERROR: DAC pointer is null");
        return false;
    }

    dac_ = dac;
    trigger_pin_pixel_ = trigger_pin_pixel;
    trigger_pin_line_ = trigger_pin_line;
    trigger_pin_frame_ = trigger_pin_frame;
    laser_pin_ = laser_pin;

    // Configure trigger pins and the laser gate pin
    uint64_t pin_mask = 0;
    if (trigger_pin_pixel_ >= 0) pin_mask |= (1ULL << trigger_pin_pixel_);
    if (trigger_pin_line_ >= 0) pin_mask |= (1ULL << trigger_pin_line_);
    if (trigger_pin_frame_ >= 0) pin_mask |= (1ULL << trigger_pin_frame_);
    if (laser_pin_ >= 0) pin_mask |= (1ULL << laser_pin_);

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
        if (laser_pin_ >= 0) gpio_set_level((gpio_num_t)laser_pin_, 0);

        SCANNER_LOG("Pins configured: Pixel=GPIO%d, Line=GPIO%d, Frame=GPIO%d, Laser=GPIO%d",
                    trigger_pin_pixel_, trigger_pin_line_, trigger_pin_frame_, laser_pin_);
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
    // Validate configuration. In bidirectional mode there is no flyback
    // (odd lines scan backwards), so fly_samples does not contribute.
    uint32_t line_total = (uint32_t)config.pre_samples +
                          2u * config.overscan_samples + config.nx +
                          (config.bidirectional ? 0 : config.fly_samples) +
                          config.line_settle_samples;
    
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
    setLaserGate(false);      // Never leave the laser on after a stop
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
    const bool bidir = (config_.bidirectional != 0);
    const uint32_t ov = config_.overscan_samples;
    const uint32_t fly = bidir ? 0 : config_.fly_samples;
    uint32_t total = (uint32_t)config_.pre_samples + 2u * ov + config_.nx +
                     fly + config_.line_settle_samples;

    if (total == 0 || total > SCANNER_MAX_LINE_SAMPLES) {
        line_len_ = 0;
        img_start_ = 0;
        img_end_ = 0;
        return;
    }

    // Per-pixel slope of the imaging ramp; the overscan regions continue this
    // exact slope beyond [x_min, x_max] so the mirror is at constant velocity
    // throughout the (triggered) imaging window.
    const float slope = (config_.nx > 1)
        ? (float)((int32_t)config_.x_max - (int32_t)config_.x_min) / (float)(config_.nx - 1)
        : 0.0f;
    const float start_x = (float)config_.x_min - slope * (float)ov; // ramp entry point
    const float end_x   = (float)config_.x_max + slope * (float)ov; // ramp exit point

    uint32_t k = 0;

    // ---- Forward profile: pre @ start_x | ov ramp | nx ramp | ov ramp | [fly] | settle ----
    for (uint32_t i = 0; i < config_.pre_samples; ++i) {
        line_x_fwd_[k++] = clamp12((int)(start_x + 0.5f));
    }
    for (uint32_t i = 0; i < ov; ++i) { // lead-in overscan: start_x -> x_min (exclusive)
        line_x_fwd_[k++] = clamp12((int)(start_x + slope * (float)i + 0.5f));
    }
    img_start_ = (uint16_t)k;
    if (config_.nx <= 1) {
        line_x_fwd_[k++] = config_.x_min;
    } else {
        for (uint32_t i = 0; i < config_.nx; ++i) {
            line_x_fwd_[k++] = clamp12((int)((float)config_.x_min + slope * (float)i + 0.5f));
        }
    }
    img_end_ = (uint16_t)k;
    for (uint32_t i = 1; i <= ov; ++i) { // lead-out overscan: x_max -> end_x
        line_x_fwd_[k++] = clamp12((int)((float)config_.x_max + slope * (float)i + 0.5f));
    }
    // Flyback (cosine ease from end_x back to start_x) - unidirectional only
    if (fly == 1) {
        line_x_fwd_[k++] = clamp12((int)(start_x + 0.5f));
    } else if (fly > 1) {
        for (uint32_t i = 0; i < fly; ++i) {
            float t = (float)i / (float)(fly - 1);
            float s = 0.5f - 0.5f * cosf((float)M_PI * t);
            line_x_fwd_[k++] = clamp12((int)(end_x + (start_x - end_x) * s + 0.5f));
        }
    }
    // Settle region: hold at the position the next line starts from
    // (start_x after flyback; end_x in bidirectional mode)
    const float settle_x = bidir ? end_x : start_x;
    for (uint32_t i = 0; i < config_.line_settle_samples; ++i) {
        line_x_fwd_[k++] = clamp12((int)(settle_x + 0.5f));
    }
    line_len_ = (uint16_t)k;

    // ---- Reverse profile (bidirectional odd lines): exact mirror in X. ----
    // Same region layout and therefore the SAME [img_start_, img_end_) window:
    // triggers stay monotonic and equidistant; only the direction of motion
    // differs (pixel j maps to x_max - slope*j). The bridge/host must flip
    // odd lines when reassembling the image.
    for (uint32_t i = 0; i < line_len_; ++i) {
        // Mirror around the scan center: x' = (start+end) - x
        float mirrored = (start_x + end_x) - (float)line_x_fwd_[i];
        line_x_rev_[i] = clamp12((int)(mirrored + 0.5f));
    }
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

void HighSpeedScannerCore::setLaserGate(bool on)
{
    if (laser_pin_ < 0) return;
    if (on) {
        GPIO.out_w1ts = (1U << laser_pin_);
    } else {
        GPIO.out_w1tc = (1U << laser_pin_);
    }
}

/**
 * Hardware pixel clock via the RMT peripheral.
 *
 * One RMT symbol encodes a full pixel cycle (high for trig_width_us, low for
 * the rest of the dwell); the peripheral loops it exactly nx times per line
 * with hardware timing, fully decoupled from the SPI/DAC software loop.
 * Requires finite TX loop count support (ESP32-S3 yes, classic ESP32 no);
 * without it the scanner falls back to software pulses.
 */
bool HighSpeedScannerCore::enableHwPixelClock()
{
#if defined(GALVO_RMT_NEW_API)
    if (hw_clock_active_) return true;
    if (trigger_pin_pixel_ < 0) return false;

    rmt_tx_channel_config_t chan_cfg = {};
    chan_cfg.gpio_num = (gpio_num_t)trigger_pin_pixel_;
    chan_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    chan_cfg.resolution_hz = 1000000; // 1 tick = 1 us
    chan_cfg.mem_block_symbols = 48;
    chan_cfg.trans_queue_depth = 2;

    rmt_channel_handle_t chan = nullptr;
    if (rmt_new_tx_channel(&chan_cfg, &chan) != ESP_OK) {
        SCANNER_LOG("ERROR: RMT channel alloc failed - using software pixel clock");
        return false;
    }
    rmt_copy_encoder_config_t enc_cfg = {};
    rmt_encoder_handle_t enc = nullptr;
    if (rmt_new_copy_encoder(&enc_cfg, &enc) != ESP_OK || rmt_enable(chan) != ESP_OK) {
        if (enc) rmt_del_encoder(enc);
        rmt_del_channel(chan);
        SCANNER_LOG("ERROR: RMT encoder/enable failed - using software pixel clock");
        return false;
    }
    rmt_chan_ = chan;
    rmt_encoder_ = enc;
    hw_clock_active_ = true;
    SCANNER_LOG("Hardware pixel clock enabled (RMT on GPIO%d)", trigger_pin_pixel_);
    return true;
#elif defined(GALVO_RMT_LEGACY_API)
    if (hw_clock_active_) return true;
    if (trigger_pin_pixel_ < 0) return false;

    rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX((gpio_num_t)trigger_pin_pixel_, GALVO_RMT_CHANNEL);
    cfg.clk_div = 80;               // 80 MHz APB / 80 -> 1 tick = 1 us
    cfg.tx_config.loop_en = true;   // loop the symbol; autostop after loop count
    if (rmt_config(&cfg) != ESP_OK ||
        rmt_driver_install(GALVO_RMT_CHANNEL, 0, 0) != ESP_OK) {
        SCANNER_LOG("ERROR: RMT (legacy) init failed - using software pixel clock");
        return false;
    }
    rmt_enable_tx_loop_autostop(GALVO_RMT_CHANNEL, true);
    hw_clock_active_ = true;
    SCANNER_LOG("Hardware pixel clock enabled (legacy RMT ch%d on GPIO%d)",
                (int)GALVO_RMT_CHANNEL, trigger_pin_pixel_);
    return true;
#else
    SCANNER_LOG("Hardware pixel clock not supported on this SoC - using software pulses");
    return false;
#endif
}

void HighSpeedScannerCore::disableHwPixelClock()
{
#if defined(GALVO_RMT_NEW_API) || defined(GALVO_RMT_LEGACY_API)
    if (!hw_clock_active_) return;
#if defined(GALVO_RMT_NEW_API)
    rmt_disable((rmt_channel_handle_t)rmt_chan_);
    rmt_del_encoder((rmt_encoder_handle_t)rmt_encoder_);
    rmt_del_channel((rmt_channel_handle_t)rmt_chan_);
    rmt_chan_ = nullptr;
    rmt_encoder_ = nullptr;
#else
    rmt_tx_stop(GALVO_RMT_CHANNEL);
    rmt_driver_uninstall(GALVO_RMT_CHANNEL);
#endif
    hw_clock_active_ = false;
    // Give the pin back to the GPIO matrix for software pulses
    if (trigger_pin_pixel_ >= 0) {
        gpio_config_t io = {};
        io.intr_type = GPIO_INTR_DISABLE;
        io.mode = GPIO_MODE_OUTPUT;
        io.pin_bit_mask = (1ULL << trigger_pin_pixel_);
        gpio_config(&io);
        gpio_set_level((gpio_num_t)trigger_pin_pixel_, 0);
    }
    SCANNER_LOG("Hardware pixel clock disabled");
#endif
}

void HighSpeedScannerCore::armHwPixelClock(uint16_t count, uint16_t period_us, uint16_t width_us)
{
#if defined(GALVO_RMT_NEW_API) || defined(GALVO_RMT_LEGACY_API)
    if (!hw_clock_active_ || count == 0 || period_us == 0) return;
    // RMT symbol durations are 15-bit (max 32767 ticks @ 1 MHz)
    uint16_t w = width_us ? width_us : 1;
    if (w >= period_us) w = (period_us > 1) ? (period_us - 1) : 1;
    uint16_t low = period_us - w;
    if (w > 32767 || low > 32767) return; // dwell too long for one symbol - skip

#if defined(GALVO_RMT_NEW_API)
    rmt_symbol_word_t sym = {};
    sym.level0 = 1;
    sym.duration0 = w;
    sym.level1 = 0;
    sym.duration1 = low;

    rmt_transmit_config_t tx_cfg = {};
    tx_cfg.loop_count = count;
    // Previous line's train is guaranteed shorter than the full line period,
    // but don't block if something went wrong
    rmt_tx_wait_all_done((rmt_channel_handle_t)rmt_chan_, 0);
    rmt_transmit((rmt_channel_handle_t)rmt_chan_, (rmt_encoder_handle_t)rmt_encoder_,
                 &sym, sizeof(sym), &tx_cfg);
#else
    rmt_item32_t item;
    item.level0 = 1;
    item.duration0 = w;
    item.level1 = 0;
    item.duration1 = low;

    rmt_tx_stop(GALVO_RMT_CHANNEL); // previous train has finished (autostop)
    rmt_set_tx_loop_count(GALVO_RMT_CHANNEL, count);
    rmt_write_items(GALVO_RMT_CHANNEL, &item, 1, false);
#endif
#endif
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
        // RASTER SCAN MODE
        // Snapshot configuration for this frame
        ScanConfig cfg;
        xSemaphoreTake(config_mutex_, portMAX_DELAY);
        cfg = config_;
        uint16_t line_len = line_len_;
        uint32_t img_start = img_start_;
        uint32_t img_end = img_end_;
        xSemaphoreGive(config_mutex_);

        if (line_len == 0 || cfg.nx == 0 || cfg.ny == 0) {
            SCANNER_LOG("Invalid configuration, stopping");
            running_ = false;
            continue;
        }

        // Hardware pixel clock lifecycle (RMT owns the pixel pin while active)
        const bool want_hw_clock = (cfg.hw_pixel_clock != 0) && (cfg.sample_period_us > 0);
        if (want_hw_clock && !hw_clock_active_) enableHwPixelClock();
        if (!want_hw_clock && hw_clock_active_) disableHwPixelClock();
        const bool hw_clock = hw_clock_active_;

        // Log frame start (only every 100 frames to avoid spam)
        if (frame_idx_ % 100 == 0) {
            SCANNER_LOG("Frame %lu starting, bidirectional=%d, hw_clock=%d",
                        (unsigned long)frame_idx_, cfg.bidirectional, (int)hw_clock);
        }

        const bool bidir = (cfg.bidirectional != 0);
        const bool do_trig = (cfg.enable_trigger != 0);
        const bool do_laser = (cfg.laser_blanking != 0) && (laser_pin_ >= 0);
        // Pixel pulse width (software mode): honour trig_width_us but always
        // leave at least half the dwell for the DAC update; 0 keeps the legacy
        // register-fast pulse (tens of ns)
        uint16_t pix_width_us = cfg.trig_width_us;
        if (cfg.sample_period_us > 0 && pix_width_us > cfg.sample_period_us / 2) {
            pix_width_us = cfg.sample_period_us / 2;
        }

        // Scan all lines (Y steps)
        for (uint16_t ly = 0; ly < cfg.ny && running_ && !config_changed_; ++ly) {
            line_idx_ = ly;

            // Bidirectional: odd lines use the mirrored profile. Trigger and
            // laser windows use the SAME sample indices in both directions.
            const bool reverse_line = bidir && (ly & 1);
            const uint16_t* line_buf = reverse_line ? line_x_rev_ : line_x_fwd_;

            // Compute Y position - lock only for the computation
            xSemaphoreTake(config_mutex_, portMAX_DELAY);
            uint16_t y12 = computeY(ly);
            bool do_lut = (config_.apply_x_lut != 0) && x_map_valid_;
            uint16_t sp_us = config_.sample_period_us;
            xSemaphoreGive(config_mutex_);

            // Set Y position once per line
            dac_->setY(y12);
            dac_->ldacPulse();

            // LINE/FRAME MARKERS at line start (during the pre-blanking hold,
            // where the DAC is static and there is timing slack). The frame
            // marker leads the line marker by trig_delay_us; both lead the
            // first pixel by (pre_samples + overscan_samples) * dwell, which
            // the acquisition side models as offset_left / trigger delay.
            if (do_trig) {
                uint16_t marker_width = cfg.trig_width_us ? cfg.trig_width_us : 1;
                if (ly == 0 && trigger_pin_frame_ >= 0) {
                    GPIO.out_w1ts = (1U << trigger_pin_frame_);
                    esp_rom_delay_us(marker_width);
                    GPIO.out_w1tc = (1U << trigger_pin_frame_);
                    if (cfg.trig_delay_us) esp_rom_delay_us(cfg.trig_delay_us);
                }
                if (trigger_pin_line_ >= 0) {
                    GPIO.out_w1ts = (1U << trigger_pin_line_);
                    esp_rom_delay_us(marker_width);
                    GPIO.out_w1tc = (1U << trigger_pin_line_);
                }
            }

            int64_t next_t = esp_timer_get_time();

            // Scan one line
            for (uint32_t i = 0; i < line_len && running_ && !config_changed_; ++i) {
                next_t += sp_us;

                // TIMING CONTROL: wait until the exact periodic instant
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
                }

                // IMAGING WINDOW ENTRY: open laser gate, arm hardware clock
                if (i == img_start) {
                    if (do_laser) setLaserGate(true);
                    if (do_trig && hw_clock) {
                        // RMT emits exactly nx hardware-timed pulses
                        armHwPixelClock(cfg.nx, sp_us, cfg.trig_width_us);
                    }
                }

                // PIXEL TRIGGER (software fallback): equidistant pacing comes
                // from next_t; width honours trig_width_us (capped at dwell/2)
                if (do_trig && !hw_clock && (i >= img_start) && (i < img_end) &&
                    trigger_pin_pixel_ >= 0) {
                    GPIO.out_w1ts = (1U << trigger_pin_pixel_);
                    if (pix_width_us) esp_rom_delay_us(pix_width_us);
                    GPIO.out_w1tc = (1U << trigger_pin_pixel_);
                }

                // IMAGING WINDOW EXIT: close laser gate
                if (do_laser && i == img_end) {
                    setLaserGate(false);
                }

                // DAC UPDATE with remaining time budget
                uint16_t x12 = line_buf[i];
                if (do_lut) x12 = applyXMap(x12);
                dac_->setX(x12);
                dac_->ldacPulse();
            }

            // Safety: never leave the laser on across line boundaries
            // (img_end == line_len when there is no post-imaging region)
            if (do_laser) setLaserGate(false);

            // Reset watchdog and yield after each line to prevent timeout
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
