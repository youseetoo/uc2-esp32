/**
 * canopen_tpdo_galvo.cpp
 *
 * T4.4 step 4c: C++ island for the galvo handling that used to live inline in
 * canopen_tpdo.c.
 *
 * The CANopen TPDO layer is C11 (canopen_tpdo.c), but the galvo controller
 * (GalvoController.h -> HighSpeedScannerCore.h -> <cmath>) is C++-only, and the
 * galvo_set_config / ScanConfig API is declared only for C++ translation units.
 * So the two galvo sections of the TPDO sync (the command-word doorbell and the
 * status push) live here in a small C++ translation unit. The two shims below
 * are extern "C" so canopen_tpdo.c (C) can call them; they are guarded by
 * GALVO_CONTROLLER (canopen_master / canopen_slave_galvo only), matching the
 * #ifdef at each call site in canopen_tpdo.c.
 *
 * OD_RAM is accessed with C linkage (CANopenModule.h includes OD.h inside
 * extern "C"); the galvo_* API is C++ (GalvoController.h), used only here. The
 * logic is identical to the pre-split code that lived in canopen_tpdo.c.
 */
#include "CANopenModule.h"   // OD_RAM (C linkage), gate macros, PinConfig.h

#ifdef GALVO_CONTROLLER

#include "GalvoController.h"
#include "uc2_hal/uc2_log.h"

// T1.3: uc2_log.h facades (log_i/log_w/log_e) need a file-local TAG. "UC2_CO"
// matches the tag canopen_tpdo.c uses for the same messages.
#define TAG "UC2_CO"

#ifdef __cplusplus
extern "C" {
#endif

// Command-word doorbell: execute a pending galvo command from
// OD_RAM.x2602_galvo_command_word and clear it back to GALVO_CMD_IDLE (0).
void canopen_tpdo_galvo_handle_command_word(void);

// Status push: build the galvo status word from galvo_is_running() and write
// it to OD_RAM.x2603_galvo_status_word on change (feeds the TPDO).
void canopen_tpdo_galvo_push_state(void);

#ifdef __cplusplus
}
#endif

void canopen_tpdo_galvo_handle_command_word(void)
{
    // Galvo command word is a doorbell: the master writes a nonzero code via
    // SDO, we execute it and clear the word back to GALVO_CMD_IDLE (0). We must
    // NOT treat 0 as a command — doing so, combined with the self-clear below,
    // made the cleared state look like a fresh "stop" on the very next sync
    // cycle and killed every scan right after it started. So: skip 0, and use
    // the dedicated GALVO_CMD_STOP code for stops.
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
                // Fall through on purpose — same raster path, ny is clamped to 1.
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

void canopen_tpdo_galvo_push_state(void)
{
    // Galvo state: push status word into OD for TPDO
    uint8_t galvoStatus = 0;
    if (galvo_is_running()) galvoStatus |= 0x01;  // bit 0: moving
    if (galvo_is_running()) galvoStatus |= 0x02;  // bit 1: scan active

    static uint8_t s_lastGalvoStatus = 0xFF;
    if (galvoStatus != s_lastGalvoStatus) {
        OD_RAM.x2603_galvo_status_word = galvoStatus;
        s_lastGalvoStatus = galvoStatus;
    }
}

#endif // GALVO_CONTROLLER
