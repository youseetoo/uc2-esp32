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

// T4.4 4e: registers the 0x2610 arbitrary-points OD extension (moved here
// from canopen_module.cpp, which is now C; GalvoController.h is C++-only).
// Called from the C co_main_task after CO_CANopenInit, guarded by
// GALVO_CONTROLLER at the call site.
void canopen_galvo_od_register(void);

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

// ============================================================================
// Galvo arbitrary points — OD extension for SDO domain transfer (slave side)
// T4.4 4e: moved from canopen_module.cpp (now C) — GalvoController.h is
// C++-only. Mirrors the LED pixel pattern: the master streams a packed byte
// buffer to 0x2610 (GALVO_POINTS_DATA); we reassemble it, parse the header +
// point list, and start an arbitrary-point scan. Wire format:
//   GalvoPointsHeader { u8 trigger_mode, u16 point_count }   (packed, 3 bytes)
//   point_count x ArbitraryScanPoint { u16 x, u16 y, u32 dwell_us }  (8 bytes)
// ============================================================================
#define GALVO_POINTS_BUF_MAX (sizeof(GalvoPointsHeader) + \
                              SCANNER_MAX_ARBITRARY_POINTS * sizeof(ArbitraryScanPoint))
static OD_extension_t s_galvoPointsExt;
static uint8_t        s_galvoPointsBuffer[GALVO_POINTS_BUF_MAX];
static size_t         s_galvoPointsBytesReceived = 0;

static ODR_t onGalvoPointsWrite(OD_stream_t* stream, const void* buf,
                                OD_size_t count, OD_size_t* countWritten) {
    if (s_galvoPointsBytesReceived + count > sizeof(s_galvoPointsBuffer)) {
        s_galvoPointsBytesReceived = 0;
        return ODR_OUT_OF_MEM;
    }
    memcpy(s_galvoPointsBuffer + s_galvoPointsBytesReceived, buf, count);
    s_galvoPointsBytesReceived += count;
    *countWritten = count;

    // Not the last segment yet — keep buffering.
    if (stream->dataOffset + count < stream->dataLength) {
        return ODR_OK;
    }

    // Final segment: parse and launch. Reset the accumulator first so a
    // malformed transfer can't wedge the next one.
    size_t total = s_galvoPointsBytesReceived;
    s_galvoPointsBytesReceived = 0;

    if (total < sizeof(GalvoPointsHeader)) {
        log_w("Galvo points: short transfer (%u bytes)", (unsigned)total);
        return ODR_OK;
    }
    GalvoPointsHeader hdr;
    memcpy(&hdr, s_galvoPointsBuffer, sizeof(hdr));
    uint16_t n = hdr.point_count;
    size_t expected = sizeof(GalvoPointsHeader) + (size_t)n * sizeof(ArbitraryScanPoint);
    if (n == 0 || n > SCANNER_MAX_ARBITRARY_POINTS || total < expected) {
        log_w("Galvo points: bad transfer count=%u total=%u expected=%u",
              (unsigned)n, (unsigned)total, (unsigned)expected);
        return ODR_OK;
    }

    ArbitraryScanPoint points[SCANNER_MAX_ARBITRARY_POINTS];
    memcpy(points, s_galvoPointsBuffer + sizeof(GalvoPointsHeader),
           (size_t)n * sizeof(ArbitraryScanPoint));
    log_i("Galvo points received: %u points, trigger_mode=%u",
          (unsigned)n, (unsigned)hdr.trigger_mode);

    if (galvo_set_arbitrary_points(points, n)) {
        galvo_set_scan_mode(SCAN_MODE_ARBITRARY);
        galvo_set_trigger_mode(hdr.trigger_mode);
        galvo_start();
    } else {
        log_w("Galvo points: setArbitraryPoints failed");
    }
    return ODR_OK;
}

void canopen_galvo_od_register(void)
{
    OD_entry_t* gpEntry = OD_find(OD, 0x2610 /* UC2_OD::GALVO_POINTS_DATA —
                                                  UC2_OD_Indices.h is C++-only,
                                                  but this IS the C++ island */);
    if (gpEntry) {
        s_galvoPointsExt.object = NULL;
        s_galvoPointsExt.read   = NULL;
        s_galvoPointsExt.write  = onGalvoPointsWrite;
        OD_extension_init(gpEntry, &s_galvoPointsExt);
        log_i("Galvo points OD extension registered (0x2610)");
    }
}

#endif // GALVO_CONTROLLER
