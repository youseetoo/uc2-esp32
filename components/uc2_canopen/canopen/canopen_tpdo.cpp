/**
 * canopen_tpdo.cpp
 * T4.4: split from CANopenModule.cpp (behavior-identical). OD<->Module RPDO sync + Module->OD TPDO sync.
 */

#include "CANopenModule.h"


#include "PinConfig.h"
#ifdef CAN_CONTROLLER_CANOPEN

#include "RuntimeConfig.h"
#include "NVSConfig.h"
#include "RoutingTable.h"
// Named OD constants — generated from tools/canopen/uc2_canopen_registry.yaml.
// Constants are declared here for compile-time verification; not yet used in logic.
#include "UC2_OD_Indices.h"

// SerialProcess is needed by the GPIO-slave TPDO2 sniffer (master-only) as
// well as the motor-completion path below, so it's hoisted out of the
// MOTOR_CONTROLLER block.
#include "SerialProcess.h"
#ifdef MOTOR_CONTROLLER
#include "FocusMotor.h"
#include "QidRegistry.h"
#endif
#ifdef HOME_MOTOR
#include "HomeMotor.h"
#endif
#ifdef LASER_CONTROLLER
#include "LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "LedController.h"
#endif
#ifdef GALVO_CONTROLLER
#include "GalvoController.h"
#endif
#ifdef TMC_CONTROLLER
#include "TMCController.h"
#endif
// Axis fault enum (header-only; safe to include on any CAN build) for the
// EMCY code map used by emitAxisEmcy / the master EMCY consumer.
#include "AxisTypes.h"
#ifdef AXIS_CONTROLLER
#include "AxisController.h"
#endif

#include "CanOpenOTA.h"

extern "C" {
#include <CANopen.h>
#include "OD.h"
}

#ifdef UNIT_TEST
// Host-test routing: SDO write/read and node-reachability checks go through
// these C shims, provided by the mock (test/unity/mocks/mock_canopen.cpp).
// The production ESP32 build never defines UNIT_TEST, so the real paths
// (CO->SDOclient, s_nodeLastSeenMs liveness) are untouched.
extern "C" {
    int  CO_writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                     uint8_t* data, size_t dataSize, int timeoutMs);
    int  CO_readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                    uint8_t* data, size_t dataSize, size_t* bytesRead,
                    int timeoutMs);
    bool CO_nodeReachable(uint8_t nodeId);
}
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"   // esp_read_mac() — populate 0x2509 MAC + 0x1018 serialNumber
#include <strings.h>   // strcasecmp() — case-insensitive MAC compare in act()
#include "driver/gpio.h"
#include "driver/twai.h"

// ============================================================================
// Constants
// ============================================================================
#define TAG_CAN    "UC2_CAN"
#define TAG_CO     "UC2_CO"
// T1.3: uc2_log.h façades (log_i/log_e/log_w/log_d) need a file-local TAG.
// TAG_CO is the dominant tag in this file; TAG_CAN is kept for the raw
// ESP_LOG* calls in the TWAI layer.
#define TAG TAG_CO

// Task priorities (same scheme as MWE trainer)
// TWAI_ctrl is the I/O pump between CAN_TX/RX_queue and the hardware. It
// blocks on driver alerts and only runs when a frame completed or arrived,
// so it must outrank the tasks that feed it — otherwise a queued frame
// waits for the next scheduler tick instead of going out back-to-back.
#define CAN_CTRL_TASK_PRIO          5
#define CANOPEN_TASK_PRIO           3
#define CANOPEN_TMR_TASK_PRIO       4
#define CANOPEN_INT_TASK_PRIO       2

// NMT control flags.
// Boot to OPERATIONAL and STAY there. We deliberately do NOT set
// CO_NMT_ERR_ON_ERR_REG (nor CO_NMT_ERR_ON_BUSOFF_HB): a UC2 node is routinely
// alone on the bus (master powered before its slaves, a slave unplugged, ...)
// with no peer to ACK its frames, so the CAN communication error bit sets.
// Tying NMT state to that demoted the node to PRE-OPERATIONAL, where it stopped
// producing PDOs/heartbeats; and because the error never clears while alone it
// got stuck there and stayed unreachable even after the peer returned (the
// "long disconnect can't recover" symptom). Bus-off is still recovered at the
// TWAI hardware layer (CAN_ctrl_task), so a node that stays OPERATIONAL simply
// resumes talking the instant the peer reappears — no NMT round-trip needed.
#define NMT_CONTROL CO_NMT_STARTUP_TO_OPERATIONAL
#define FIRST_HB_TIME           500
// SDO server timeout: must be generous for block download OTA.
// The slave's SDO server aborts with 0x05040000 if it doesn't receive
// the next expected frame within this period.  1000ms was too tight —
// the master needs time to process the initiate response and begin
// sending sub-blocks (CAN_ctrl_task drain rate + task scheduling jitter).
#define SDO_SRV_TIMEOUT_TIME   5000
#define SDO_CLI_TIMEOUT_TIME   5000
#define SDO_CLI_BLOCK          false
#define OD_STATUS_BITS         NULL

#include "canopen_module_priv.h"

// ============================================================================
// OD <-> Module sync (called from CO_tmr_task)
// Using native UC2 OD entries (0x2000+)
// We poll this every 1ms from the timer task for simplicity and responsiveness; 
// no interrupts or mutexes needed for this one-way communication from OD to Module.
// ============================================================================
void CANopenModule::syncRpdoToModules()
{
    if (runtime_config_is_slave(&runtimeConfig)) {
        syncRpdoToModules_slave();
    } else {
        syncRpdoToModules_master();
    }
}

// Slave: consume RPDO data from OD_RAM and dispatch to local modules
void CANopenModule::syncRpdoToModules_slave()
{
    // Skip motor housekeeping during OTA — avoids competing with the SDO task.
    if (OD_OTA_STATUS == CANOPEN_OTA_RECEIVING ||
        OD_OTA_STATUS == CANOPEN_OTA_VERIFYING) return;

#ifdef MOTOR_CONTROLLER
    uint8_t cmdWord = OD_RAM.x2003_motor_command_word;
    if (cmdWord) {
        for (int ax = 0; ax < 4; ax++) {
            if (cmdWord & (1u << ax)) {
                // Move command for axis ax
                log_i("Received motor command for axis %d: pos=%ld spd=%ld accel=%ld abs=%d forever=%d (cmdWord=0x%02x)",
                    ax, (long)OD_RAM.x2000_motor_target_position[ax],
                    (long)OD_RAM.x2002_motor_speed[ax],
                    (long)OD_RAM.x2006_motor_acceleration[ax],
                    OD_RAM.x2007_motor_is_absolute[ax] != 0,
                    OD_RAM.x200B_motor_is_forever[ax] != 0,
                    cmdWord);
                s_axisCmds[ax].pos   = OD_RAM.x2000_motor_target_position[ax];
                s_axisCmds[ax].speed = (int32_t)OD_RAM.x2002_motor_speed[ax];
                s_axisCmds[ax].accel = (int32_t)OD_RAM.x2006_motor_acceleration[ax];
                s_axisCmds[ax].isAbs     = OD_RAM.x2007_motor_is_absolute[ax] != 0;
                s_axisCmds[ax].isForever = OD_RAM.x200B_motor_is_forever[ax] != 0;
                s_axisCmds[ax].isStop    = false;
                s_axisCmds[ax].pending   = true;  // commit last
            }
            if (cmdWord & (1u << (ax + 4))) {
                log_i("Received STOP command for axis %d (cmdWord=0x%02x)", ax, cmdWord);
                // Stop command for axis ax
                s_axisCmds[ax].isStop  = true;
                s_axisCmds[ax].pending = true;
            }
        }
        OD_RAM.x2003_motor_command_word = 0;
    }

    // Motor enable: detect change on the local axis and apply to driver.
    // setEnable() is global (affects all FAS steppers on this board), so we
    // only watch the entry corresponding to the local axis ID.
    {
        uint8_t localAx = (uint8_t)pinConfig.REMOTE_MOTOR_AXIS_ID;
        if (localAx < 4) {
            static uint8_t s_lastMotorEnable = 0xFF;
            uint8_t en = OD_RAM.x2005_motor_enable[localAx];
            if (en != s_lastMotorEnable) {
                s_lastMotorEnable = en;
                log_i("Motor enable change ax=%u -> %u", localAx, en);
                focusmotor_set_enable(en != 0);
            }
        }
    }

    // Throttle STALE-pending warnings: this is called from CO_tmr_task at 1ms,
    // logging every tick floods the serial port and starves IDLE0 -> task watchdog
    // reset. Only warn once per axis if the pending hasn't been consumed for >200 ms.
    {
        static uint32_t s_pendSince[4] = {0,0,0,0};
        static uint32_t s_lastWarn[4]  = {0,0,0,0};
        uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
        for (int ax = 0; ax < 4; ax++) {
            if (s_axisCmds[ax].pending) {
                if (s_pendSince[ax] == 0) s_pendSince[ax] = nowMs;
                if (nowMs - s_pendSince[ax] > 200 && nowMs - s_lastWarn[ax] > 1000) {
                    s_lastWarn[ax] = nowMs;
                    ESP_LOGW(TAG_CO, "STALE pending ax=%d pos=%ld spd=%ld stop=%d (cmdWord=0x%02x)",
                             ax, (long)s_axisCmds[ax].pos, (long)s_axisCmds[ax].speed,
                             s_axisCmds[ax].isStop, cmdWord);
                }
            } else {
                s_pendSince[ax] = 0;
                s_lastWarn[ax]  = 0;
            }
        }
    }
#endif

#ifdef AXIS_CONTROLLER
    // Axis closed-loop commands (design v2, WP8). The SDO server has written the
    // host's values into OD_RAM; here we detect changes/doorbells and hand them
    // to AxisController as DEFERRED requests (the actual work — incl. blocking
    // calibration — runs on the main loop task, never here in the CO timer task).
    {
        // Sub-index tolerance: CANopenNode maps SDO sub i -> array slot i-1, and
        // hosts variously address a single-axis slave with sub 1 (slot 0) or
        // sub REMOTE_MOTOR_AXIS_ID+1. Per the OD-axis-remap rule, accept a
        // command in ANY slot and apply it to the ONE physical axis of this
        // node, then re-sync all slots so stale slots never re-trigger.
        int ax = (int)pinConfig.REMOTE_MOTOR_AXIS_ID;
        if (ax >= 0 && ax < 4) {
            uint8_t curMode = axis_get_feedback(ax).mode;
            // First pass after boot: seed the OD slots with the controller's
            // actual mode (MONITOR by default when calibrated). Without this,
            // the all-zero boot OD would immediately force OPEN_LOOP.
            static bool s_axisModeSeeded = false;
            if (!s_axisModeSeeded) {
                for (int k = 0; k < 4; k++)
                    OD_RAM.x2042_axis_mode[k] = curMode;
                s_axisModeSeeded = true;
            }
            // MODE (0x2042): apply the first slot that differs from the current
            // mode, then mirror the new mode into every slot.
            for (int s = 0; s < 4; s++) {
                uint8_t desiredMode = OD_RAM.x2042_axis_mode[s];
                if (desiredMode <= 3 && desiredMode != curMode) {
                    axis_request_mode(ax, desiredMode);
                    for (int k = 0; k < 4; k++)
                        OD_RAM.x2042_axis_mode[k] = desiredMode;
                    break;
                }
            }
            // RESET doorbell (0x2045): 1=TRUST_ENCODER 2=TRUST_STEPS 3=FORCE_REHOME.
            for (int s = 0; s < 4; s++) {
                uint8_t rst = OD_RAM.x2045_axis_reset[s];
                if (rst != 0) {
                    uint8_t policy = (rst >= 1 && rst <= 3) ? (uint8_t)(rst - 1) : 0;
                    axis_request_reset(ax, policy);
                    for (int k = 0; k < 4; k++)
                        OD_RAM.x2045_axis_reset[k] = 0; // clear doorbells
                    break;
                }
            }
            // CALIBRATE doorbell (0x2048): trigger the routine.
            for (int s = 0; s < 4; s++) {
                if (OD_RAM.x2048_axis_calibrate[s] != 0) {
                    axis_request_calibration(ax);
                    for (int k = 0; k < 4; k++)
                        OD_RAM.x2048_axis_calibrate[k] = 0; // clear doorbells
                    break;
                }
            }
        }
    }
#endif

#ifdef HOME_MOTOR
    for (int ax = 0; ax < 4; ax++) {
        if (OD_RAM.x2010_homing_command[ax]) {
            s_homingCmds[ax].speed          = (int32_t)OD_RAM.x2011_homing_speed[ax];
            s_homingCmds[ax].direction      = OD_RAM.x2012_homing_direction[ax];
            s_homingCmds[ax].timeout        = (int32_t)OD_RAM.x2013_homing_timeout[ax];
            s_homingCmds[ax].endstopRelease = OD_RAM.x2014_homing_endstop_release[ax];
            s_homingCmds[ax].polarity       = OD_RAM.x2015_homing_endstop_polarity[ax];
            s_homingCmds[ax].pending        = true;
            OD_RAM.x2010_homing_command[ax] = 0;
        }
    }
#endif

#ifdef MOTOR_CONTROLLER
    // TODO: Is this actually working? 
    // Hard-limit config and clear-command
    for (int ax = 0; ax < 4; ax++) {
        // command word: 1 = clear triggered flag + lockout
        if (OD_RAM.x2030_hardlimit_command[ax]) {
            s_hardlimitCmds[ax].isCommand = true;
            s_hardlimitCmds[ax].isConfig  = false;
            s_hardlimitCmds[ax].pending   = true;
            OD_RAM.x2030_hardlimit_command[ax] = 0;
        }
        // enabled/polarity — arm on any write (master sets these before command)
        static uint8_t lastEn[4]  = {0xFF, 0xFF, 0xFF, 0xFF};
        static uint8_t lastPol[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        static bool    hlInitialized[4] = {false, false, false, false};
        // On first sync, prime lastEn/lastPol from OD_RAM without dispatching a
        // CONFIG. This prevents the slave from overwriting the hardlimit values
        // loaded by FocusMotor::fill_data() (preferences fallback to pinConfig
        // defaults — e.g. enabled-by-default on canopen_slave_motor) with the
        // OD's zero-initialised contents on every reboot.
        if (!hlInitialized[ax]) {
            lastEn[ax]  = OD_RAM.x2031_hardlimit_enabled[ax];
            lastPol[ax] = OD_RAM.x2032_hardlimit_polarity[ax];
            hlInitialized[ax] = true;
            // Also push the locally-loaded values into the OD so masters that
            // SDO-read them get the actual active configuration. We only do
            // this for the local axis owned by this slave.
            if ((uint8_t)pinConfig.REMOTE_MOTOR_AXIS_ID == (uint8_t)ax) {
                MotorData* md = focusmotor_get_data()[ax];
                if (md) {
                    OD_RAM.x2031_hardlimit_enabled[ax]  = md->hardLimitEnabled  ? 1 : 0;
                    OD_RAM.x2032_hardlimit_polarity[ax] = md->hardLimitPolarity ? 1 : 0;
                    lastEn[ax]  = OD_RAM.x2031_hardlimit_enabled[ax];
                    lastPol[ax] = OD_RAM.x2032_hardlimit_polarity[ax];
                }
            }
        }
        if (OD_RAM.x2031_hardlimit_enabled[ax]  != lastEn[ax]  ||
            OD_RAM.x2032_hardlimit_polarity[ax] != lastPol[ax]) {
            lastEn[ax]  = OD_RAM.x2031_hardlimit_enabled[ax];
            lastPol[ax] = OD_RAM.x2032_hardlimit_polarity[ax];
            s_hardlimitCmds[ax].enabled  = lastEn[ax];
            s_hardlimitCmds[ax].polarity = lastPol[ax];
            s_hardlimitCmds[ax].isConfig  = true;
            s_hardlimitCmds[ax].isCommand = false;
            s_hardlimitCmds[ax].pending   = true;
        }
    }
#endif

#ifdef LASER_CONTROLLER
    for (int ch = 0; ch < 4; ch++) {
        uint16_t v = OD_RAM.x2100_laser_pwm_value[ch];
        // Only arm pending when the OD value actually changed from the last dispatched value.
        // Using OR (!pending || value!=v) caused an infinite loop: loop() clears pending,
        // syncRpdoToModules() immediately re-arms it with the same value, ad infinitum.
        if (s_laserCmds[ch].value != v) {
            s_laserCmds[ch].value   = v;
            s_laserCmds[ch].pending = true;
        }
    }
#endif

#ifdef LED_CONTROLLER
    // LED mode: detect changes to mode/brightness/colour and dispatch
        if (0)
            log_i("LED mode check: mode=%u bright=%u colour=0x%06X",
                (unsigned)OD_RAM.x2200_led_array_mode,
                (unsigned)OD_RAM.x2201_led_brightness,
                (unsigned)OD_RAM.x2202_led_uniform_colour);
        static uint8_t  lastLedMode   = 0xFF;
        static uint8_t  lastLedBright = 0xFF;
        static uint32_t lastLedColour = 0xFFFFFFFF;
        uint8_t  curMode   = OD_RAM.x2200_led_array_mode;
        uint8_t  curBright = OD_RAM.x2201_led_brightness;
        uint32_t curColour = OD_RAM.x2202_led_uniform_colour;
        if (curMode != lastLedMode || curBright != lastLedBright || curColour != lastLedColour) {
            lastLedMode   = curMode;
            lastLedBright = curBright;
            lastLedColour = curColour;
            led_set_mode(curMode, curBright, curColour);
        }
    
    // LED pattern: detect changes to pattern id/speed
        static uint8_t  lastPatternId    = 0xFF;
        static uint16_t lastPatternSpeed = 0xFFFF;
        uint8_t  curPat   = OD_RAM.x2220_led_pattern_id;
        uint16_t curSpeed = OD_RAM.x2221_led_pattern_speed;
        if (curPat != lastPatternId || curSpeed != lastPatternSpeed) {
            lastPatternId    = curPat;
            lastPatternSpeed = curSpeed;
            led_set_pattern(curPat, curSpeed);
        }
    
#endif

#ifdef GALVO_CONTROLLER
    // Galvo command word is a doorbell: the master writes a nonzero code via
    // SDO, we execute it and clear the word back to GALVO_CMD_IDLE (0). We must
    // NOT treat 0 as a command — doing so, combined with the self-clear below,
    // made the cleared state look like a fresh "stop" on the very next sync
    // cycle and killed every scan right after it started. So: skip 0, and use
    // the dedicated GALVO_CMD_STOP code for stops.
    {
        uint8_t cmd = OD_RAM.x2602_galvo_command_word;
        if (cmd != GALVO_CMD_IDLE) {
            switch (cmd) {
                case GALVO_CMD_GOTO: // Goto XY / park — stops any scan, drives DAC directly
                    log_i("Galvo goto command: x=%u y=%u",
                          (unsigned)OD_RAM.x2600_galvo_target_position[0],
                          (unsigned)OD_RAM.x2600_galvo_target_position[1]);
                    galvo_goto_xy((uint16_t)OD_RAM.x2600_galvo_target_position[0],
                                  (uint16_t)OD_RAM.x2600_galvo_target_position[1]);
                    break;
                case GALVO_CMD_LINE: // Line scan (single line: ny forced to 1 below)
                    log_i("Galvo line scan command received");
                    // fall through — same raster path, ny is clamped to 1
                    [[fallthrough]];
                case GALVO_CMD_RASTER: { // Raster scan
                    log_i("Galvo raster scan command received: x_start=%u y_start=%u x_steps=%u y_steps=%u x_step=%d y_step=%d speed=%u pre_us=%u post_us=%u trigger=%u",
                          (unsigned)OD_RAM.x260B_galvo_x_start,
                          (unsigned)OD_RAM.x260C_galvo_y_start,
                          (unsigned)OD_RAM.x2605_galvo_n_steps_line,
                          (unsigned)OD_RAM.x2606_galvo_n_steps_pixel,
                          (int32_t)OD_RAM.x260D_galvo_x_step,
                          (int32_t)OD_RAM.x260E_galvo_y_step,
                          (unsigned)OD_RAM.x2604_galvo_scan_speed,
                          (unsigned)OD_RAM.x2609_galvo_t_pre_us,
                          (unsigned)OD_RAM.x260A_galvo_t_post_us,
                          (unsigned)OD_RAM.x260F_galvo_camera_trigger_mode);
                    ScanConfig cfg;
                    cfg.x_min = (uint16_t)OD_RAM.x260B_galvo_x_start;
                    cfg.y_min = (uint16_t)OD_RAM.x260C_galvo_y_start;
                    cfg.nx = OD_RAM.x2605_galvo_n_steps_line;
                    cfg.ny = (cmd == GALVO_CMD_LINE) ? 1 : OD_RAM.x2606_galvo_n_steps_pixel;
                    int32_t xStep = OD_RAM.x260D_galvo_x_step;
                    int32_t yStep = OD_RAM.x260E_galvo_y_step;
                    cfg.x_max = (uint16_t)(cfg.x_min + cfg.nx * xStep);
                    cfg.y_max = (uint16_t)(cfg.y_min + cfg.ny * yStep);
                    cfg.sample_period_us = (uint16_t)OD_RAM.x2604_galvo_scan_speed;
                    cfg.pre_samples = OD_RAM.x2609_galvo_t_pre_us;
                    cfg.fly_samples = OD_RAM.x260A_galvo_t_post_us;
                    cfg.frame_count = 0; // Infinite until stopped
                    cfg.enable_trigger = OD_RAM.x260F_galvo_camera_trigger_mode;
                    // bidirectional + line_settle_samples ride in the reused
                    // d_steps slots (see DeviceRouter::handleGalvoAct). Line scan
                    // (single line) is always unidirectional.
                    cfg.bidirectional = (cmd == GALVO_CMD_LINE) ? 0
                                        : (uint8_t)(OD_RAM.x2607_galvo_d_steps_line ? 1 : 0);
                    cfg.line_settle_samples = OD_RAM.x2608_galvo_d_steps_pixel;
                    log_i("Galvo raster extras: bidirectional=%u line_settle=%u",
                          (unsigned)cfg.bidirectional, (unsigned)cfg.line_settle_samples);
                    if (!galvo_set_config(&cfg)) {
                        // Rejected (line > SCANNER_MAX_LINE_SAMPLES, bad range, ...):
                        // do NOT start(), that would re-arm the previous scan.
                        log_e("Galvo raster config rejected, not starting");
                        break;
                    }
                    // Reset to raster mode — a prior arbitrary-point scan (0x2610)
                    // would otherwise leave the scanner in ARBITRARY mode.
                    galvo_set_scan_mode(SCAN_MODE_RASTER);
                    galvo_start();
                    break;
                }
                case GALVO_CMD_STOP: // Stop
                    log_i("Galvo stop command received");
                    galvo_stop();
                    break;
                case GALVO_CMD_ESTOP: // Emergency stop
                    log_i("Galvo emergency stop command received");
                    galvo_stop();
                    break;
                default:
                    log_w("Galvo unknown command word: %u", (unsigned)cmd);
                    break;
            }
            OD_RAM.x2602_galvo_command_word = GALVO_CMD_IDLE;
        }
    }
#endif

#ifdef TMC_CONTROLLER
    // TMC params: detect change of any tracked OD entry on the local axis
    // and dispatch the new TMCData to TMCController. Comparing each field
    // separately avoids spurious reapplies when the master writes only one
    // entry at a time (each SDO write here lands one tick at a time).
    {
        uint8_t localAx = (uint8_t)pinConfig.REMOTE_MOTOR_AXIS_ID;
        if (localAx < 4) {
            static uint16_t lastMs    = 0xFFFF;
            static uint16_t lastCur   = 0xFFFF;
            static uint8_t  lastSg    = 0xFF;
            static uint8_t  lastSemin = 0xFF;
            static uint8_t  lastSemax = 0xFF;
            static uint8_t  lastBlank = 0xFF;
            static uint8_t  lastToff  = 0xFF;
            uint16_t ms    = OD_RAM.x2020_tmc_microsteps[localAx];
            uint16_t cur   = OD_RAM.x2021_tmc_rms_current[localAx];
            uint8_t  sg    = OD_RAM.x2022_tmc_stallguard_threshold[localAx];
            uint8_t  semin = OD_RAM.x2023_tmc_coolstep_semin[localAx];
            uint8_t  semax = OD_RAM.x2024_tmc_coolstep_semax[localAx];
            uint8_t  blank = OD_RAM.x2025_tmc_blank_time[localAx];
            uint8_t  toff  = OD_RAM.x2026_tmc_toff[localAx];

            if (ms != lastMs || cur != lastCur || sg != lastSg
                || semin != lastSemin || semax != lastSemax
                || blank != lastBlank || toff != lastToff) {
                lastMs    = ms;
                lastCur   = cur;
                lastSg    = sg;
                lastSemin = semin;
                lastSemax = semax;
                lastBlank = blank;
                lastToff  = toff;

                TMCData p{};
                // Only forward fields with non-zero values; the OD initial
                // state is all-zero, applying that to the driver would brick
                // the stepper. Zero-valued fields keep the existing values.
                p.msteps      = ms    ? ms    : pinConfig.tmc_microsteps;
                p.rms_current = cur   ? cur   : pinConfig.tmc_rms_current;
                p.sgthrs      = sg    ? sg    : pinConfig.tmc_sgthrs;
                p.semin       = semin ? semin : pinConfig.tmc_semin;
                p.semax       = semax ? semax : pinConfig.tmc_semax;
                p.blank_time  = blank ? blank : pinConfig.tmc_blank_time;
                p.toff        = toff  ? toff  : pinConfig.tmc_toff;
                // Fields not in the OD keep their pinConfig defaults
                p.stall_value = pinConfig.tmc_stall_value;
                p.sedn        = pinConfig.tmc_sedn;
                p.tcoolthrs   = pinConfig.tmc_tcoolthrs;
                // Chopper mode / hold current are not carried over CANopen yet.
                // They MUST be filled in here — p is zero-initialised, and a
                // zero hold_mult_pct would drop the standstill current to 0.
                p.tpwmthrs_sps   = pinConfig.tmc_tpwmthrs_sps;
                p.en_spreadcycle = pinConfig.tmc_en_spreadcycle;
                p.hold_mult_pct  = pinConfig.tmc_hold_multiplier_pct;
                p.hstrt          = pinConfig.tmc_hstrt;
                p.hend           = pinConfig.tmc_hend;

                log_i("TMC OD change ax=%u msteps=%u current=%u sgthrs=%u",
                      localAx, ms, cur, sg);
                tmc_apply_params(&p, true);
            }
        }
    }
#endif

#ifdef CAN_CONTROLLER_CANOPEN
    // Reboot command: master writes 1 to 0x2507; slave performs ESP.restart().
    {
        uint8_t v = OD_RAM.x2507_reboot_command;
        if (v != 0) {
            log_i("Reboot command received via CANopen — restarting in 200 ms");
            OD_RAM.x2507_reboot_command = 0;
            // Defer slightly so the SDO server can complete its response frame
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();  // T1.3: raw IDF (esp_system.h already included)
        }
    }

    // Commanded node-id (0x250A): master writes 1..127 to reassign this node's
    // CAN id (provisioning, e.g. keyed by MAC). Persist to NVS and trigger a
    // CANopen comm reset so the new id takes effect without a full reboot — same
    // mechanism as the local /can_act nodeId change. The reset is deferred ~250 ms
    // so the SDO write's response frame is sent on the *current* id before the
    // stack re-inits on the new one (otherwise the master's writeSDO times out
    // even when the assignment succeeded). vTaskDelay is avoided here so the 1 ms
    // CO_tmr_task loop keeps servicing PDOs/SDO in the meantime.
    {
        static uint32_t pendingResetAtMs = 0;
        uint8_t newId = OD_RAM.x250A_commanded_node_id;
        if (pendingResetAtMs == 0 && newId != 0) {
            if (newId <= 127 && newId != runtimeConfig.canNodeId) {
                log_i("Commanded node-id %u via CANopen (was %u) — save + deferred reset",
                      newId, runtimeConfig.canNodeId);
                runtimeConfig.canNodeId = newId;
                nvsconfig_save();
                pendingResetAtMs = millis() + 250;
            }
            OD_RAM.x250A_commanded_node_id = 0;  // consume (also clears no-op/out-of-range)
        }
        if (pendingResetAtMs != 0 && (int32_t)(millis() - pendingResetAtMs) >= 0) {
            pendingResetAtMs = 0;
            s_requestedReset = CO_RESET_COMM;
        }
    }
#endif
}

// Master: drain RPDO-written OD_RAM entries into the remote slave cache.
// Called every 1 ms from CO_tmr_task. Detects running->stopped transitions
// and emits async serial notifications.
RemoteSlaveState CANopenModule::s_remoteSlaves[REMOTE_SLAVE_SLOTS] = {};

void CANopenModule::syncRpdoToModules_master()
{
#ifdef MOTOR_CONTROLLER
    for (uint8_t slot = 0; slot < REMOTE_SLAVE_SLOTS; slot++) {
        int32_t newPos    = OD_RAM.x2001_motor_actual_position[slot];
        uint8_t newStatus = OD_RAM.x2004_motor_status_word[slot];
        uint8_t newHoming = OD_RAM.x2016_homing_status[slot];

        RemoteSlaveState& slave = s_remoteSlaves[slot];

        // Skip if nothing changed AND we've already initialised
        if (slave.seen
            && newPos    == slave.motorPosition
            && newStatus == slave.motorStatus
            && newHoming == slave.homingStatus) continue;

        bool wasRunning = slave.seen && (slave.motorStatus & 0x01) != 0;
        bool nowRunning = (newStatus & 0x01) != 0;
        bool firstSeen  = !slave.seen;
        uint8_t prevHoming = slave.seen ? slave.homingStatus : 0;

        slave.motorPosition = newPos;
        slave.motorStatus   = newStatus;
        slave.homingStatus  = newHoming;
        slave.lastUpdateMs  = millis();
        slave.seen          = true;

        // Rate-limit log to once per second (keepalive toggles arrive every 2 s)
        static uint32_t s_lastRpdoLogMs = 0;
        uint32_t nowLog = millis();
        if ((nowLog - s_lastRpdoLogMs) >= 1000) {
            s_lastRpdoLogMs = nowLog;
            /*
            log_i("Remote slave slot %d update: pos=%ld status=0x%02X running=%d",
                  slot, (long)newPos, newStatus, (int)nowRunning);
                  */
        }

        // Mirror into FocusMotor data so /motor_get keeps working
        for (uint8_t logicalAx = 0; logicalAx < 4; logicalAx++) { // TODO: We should consider having more axis 
            const auto* route = UC2::RoutingTable::find(
                UC2::RouteEntry::MOTOR, logicalAx);
            if (!route || route->where != UC2::RouteEntry::REMOTE) continue;
            if (route->subAxis != slot) continue;

            MotorData* md = focusmotor_get_data()[logicalAx];
            if (md) {
                md->currentPosition = newPos;
                md->stopped         = !nowRunning;
            }

            // Detect running -> stopped transition -> emit position JSON
            // (matches FocusMotor::sendMotorPos format) and report completion via QidRegistry.
            // ImSwitch / PS4 controller path watch for {"steppers":[{...,"isDone":1}]}.
            if (wasRunning && !nowRunning) {
                log_i("Slave slot %d (motor %d) DONE pos=%ld",
                         slot, logicalAx, (long)newPos);

                cJSON* root = cJSON_CreateObject();
                if (root) {
                    cJSON* stprs = cJSON_AddArrayToObject(root, "steppers");
                    cJSON* item  = cJSON_CreateObject();
                    if (stprs && item) {
                        cJSON_AddNumberToObject(item, "stepperid", logicalAx);
                        cJSON_AddNumberToObject(item, "position",  newPos);
                        cJSON_AddNumberToObject(item, "isDone",    1);
                        cJSON_AddItemToArray(stprs, item);
                        cJSON_AddNumberToObject(root, "qid", md ? md->qid : -1);
                        char* jsonStr = cJSON_PrintUnformatted(root);
                        if (jsonStr) {
                            serial_out_safe_send_json_string(jsonStr);
                            free(jsonStr);
                        }
                    } else if (item) {
                        cJSON_Delete(item);
                    }
                    cJSON_Delete(root);
                }

                if (md && md->qid > 0) {
                    qid_report_action_done(md->qid);
                }
            }

#ifdef HOME_MOTOR
            // Remote homing-done push: the slave reports its homing phase via OD
            // 0x2016 on TPDO1 (0=idle,1=homing,2=done,3=timeout). When a finished
            // result (>=2) arrives for an axis this master dispatched a home to,
            // emit the SAME {"home":{...},"qid":N} event the LOCAL path produces —
            // reusing home_send_done(), which reads the mirrored
            // currentPosition set just above and the qid cached at dispatch — and
            // stop the master-side timeout watchdog. Gating on homeIsActive avoids
            // spurious events from a slave that homed on its own and guarantees a
            // single report per run.
            if (md && newHoming >= 2 && newHoming != prevHoming) {
                HomeData** hd = home_get_data();
                if (hd && hd[logicalAx] && hd[logicalAx]->homeIsActive) {
                    const char* hstatus = (newHoming == 3) ? "timeout" : "done";
                    log_i("Remote home %s: slot %d (motor %d) pos=%ld",
                          hstatus, slot, logicalAx, (long)newPos);
                    home_send_done(logicalAx, hstatus);  // reads md->currentPosition + hd->qid
                    hd[logicalAx]->homeIsActive = false;
                    md->isHoming = false;
                }
            }
#endif

            if (firstSeen) {
                log_i("Slave slot %d (motor %d) FIRST SEEN pos=%ld running=%d",
                         slot, logicalAx, (long)newPos, (int)nowRunning);
            }
        }
    }
#endif
}

void CANopenModule::syncModulesToTpdo()
{
    // Skip TPDO updates during OTA — avoids competing with the SDO task.
    if (OD_OTA_STATUS == CANOPEN_OTA_RECEIVING ||
        OD_OTA_STATUS == CANOPEN_OTA_VERIFYING) return;

    /*
    This function pushes the current module state into the TPDOs.
    It is called every 1 ms from the CO_tmr_task, but only updates 
    the TPDO data when there are changes to report (e.g. motor position/status changes).

    */
    // Always update system stats (useful for all roles)
    OD_RAM.x2503_uptime_seconds = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
    OD_RAM.x2504_free_heap_bytes = (uint32_t)esp_get_free_heap_size();

    // Only slaves need to push module state into OD for TPDOs
    if (!runtime_config_is_slave(&runtimeConfig)) return;

#ifdef MOTOR_CONTROLLER
    int localAxis = (int)pinConfig.REMOTE_MOTOR_AXIS_ID;
    focusmotor_update_data(localAxis);
    MotorData* md = focusmotor_get_data()[localAxis];
    if (md == nullptr) return;

    bool isRunning = focusmotor_is_running(localAxis);
    int32_t newPos = md->currentPosition;
    uint8_t newStatus = isRunning ? 0x01u : 0x00u;

    // Heartbeat toggle: flip bit 1 of statusWord every 1 s so that the
    // master's RPDO consumer always sees a data change and refreshes
    // lastUpdateMs, even when the motor is idle. Bit 0 = running (semantic),
    // bit 1 = heartbeat toggle (ignored by master when evaluating running).
    static uint8_t  s_heartbeatToggle = 0;
    static uint32_t s_lastKeepaliveMs = 0;
    uint32_t nowMs = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if ((nowMs - s_lastKeepaliveMs) >= 1000) {
        s_heartbeatToggle ^= 0x02;  // toggle bit 1
        s_lastKeepaliveMs  = nowMs;
    }
    newStatus |= s_heartbeatToggle;

    // Homing phase, pushed alongside motor state so the master can fire an async
    // home-done event (see syncRpdoToModules_master). 0=idle,1=homing,2=done,3=timeout.
    // Latched in home_send_done()/cleared in home_start(), so a finished
    // result stays readable until the next run even though homeIsActive flips off.
    uint8_t newHoming = 0;
#ifdef HOME_MOTOR
    {
        HomeData** hd = home_get_data();
        if (hd && hd[localAxis])
            newHoming = hd[localAxis]->homeIsActive ? 1 : hd[localAxis]->homeResultCode;
    }
#endif

    static int32_t s_lastPos    = INT32_MIN;
    static uint8_t s_lastStatus = 0xFF;
    static uint8_t s_lastHoming = 0xFF;

    if (newPos != s_lastPos || newStatus != s_lastStatus || newHoming != s_lastHoming) {
        for (int ax = 0; ax < 4; ax++) {
            OD_RAM.x2001_motor_actual_position[ax] = newPos;
            OD_RAM.x2004_motor_status_word[ax]     = newStatus;
            OD_RAM.x2016_homing_status[ax]         = newHoming;
        }
        s_lastPos    = newPos;
        s_lastStatus = newStatus;
        s_lastHoming = newHoming;

        // Request TPDO1 transmission on next CO_process_TPDO cycle
        if (CO != NULL && CO->TPDO != NULL) {
            CO_TPDOsendRequest(&CO->TPDO[0]);
        }

        static uint32_t s_lastSyncLogMs = 0;
        if (nowMs - s_lastSyncLogMs >= 1000) {
            s_lastSyncLogMs = nowMs;
            /*
            (TAG_CO, "syncModulesToTpdo: local-ax%d pos=%ld running=%d -> TPDO1",
                     localAxis, (long)newPos, (int)isRunning);
                     */
        }
    }

#ifdef AXIS_CONTROLLER
    // Push AxisController feedback into the SDO-readable AXIS block (0x2040+) so
    // the master can poll it. Read-back only (measured/error/health/fault/etc.);
    // x2042 mode is a command input and is NOT written back here to avoid
    // clobbering a pending host mode change. All values in STEPS except raw.
    if (localAxis >= 0 && localAxis < 4) {
        AxisFeedback afb = axis_get_feedback(localAxis);
        AxisCalibration acal = axis_get_calibration(localAxis);
        OD_RAM.x2040_axis_measured_steps[localAxis]       = afb.measuredSteps;
        OD_RAM.x2041_axis_position_error_steps[localAxis] = afb.positionErrorSteps;
        OD_RAM.x2043_axis_health[localAxis]               = afb.health;
        OD_RAM.x2044_axis_fault[localAxis]                = afb.fault;
        OD_RAM.x2046_axis_calibrated[localAxis]           = afb.calibrated ? 1 : 0;
        OD_RAM.x2047_axis_referenced[localAxis]           = afb.referenced ? 1 : 0;
        OD_RAM.x2049_axis_counts_per_step_q16[localAxis]  = acal.countsPerStep_q16 * acal.countSign;
        OD_RAM.x204A_axis_backlash_steps[localAxis]       = axisCountsToSteps(&acal, acal.backlashCounts);
        OD_RAM.x204B_axis_raw_counts[localAxis]           = (int32_t)afb.rawCounts;
    }
#endif
#endif

#ifdef GALVO_CONTROLLER
    // Galvo state: push status word into OD for TPDO
    {
        uint8_t galvoStatus = 0;
        if (galvo_is_running()) galvoStatus |= 0x01;  // bit 0: moving
        if (galvo_is_running()) galvoStatus |= 0x02;  // bit 1: scan active

        static uint8_t s_lastGalvoStatus = 0xFF;
        if (galvoStatus != s_lastGalvoStatus) {
            OD_RAM.x2603_galvo_status_word = galvoStatus;
            s_lastGalvoStatus = galvoStatus;
        }
    }
#endif
}

#endif // CAN_CONTROLLER_CANOPEN
