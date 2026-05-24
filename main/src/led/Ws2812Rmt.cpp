#include "Ws2812Rmt.h"
#include <Arduino.h>
#include "esp_debug_helpers.h"
// Guard: only built on ESP32 family (classic or S3).
#ifdef ESP_PLATFORM

// Bare-metal WS2812 driver for the ESP32 RMT peripheral.
//
// Why not use the IDF rmt_driver_install?
// FastAccelStepper (gin66) registers its own shared ISR via rmt_isr_register
// AND writes global RMT config bits. rmt_driver_install() from another library
// would install a competing ISR on the same shared IRQ -- causing FAS to emit
// garbage step pulses.
//
// This driver masks our channel's RMT interrupts so FAS's shared ISR ignores
// our events, fills RMTMEM directly, and polls for completion. No IDF driver.
// No ISR. No global RMT config writes (except sys_conf.apb_fifo_mask on S3
// which FAS also sets to the same value).
//
// Chip differences:
//   ESP32-classic: conf_ch[N].conf0/conf1, 64 items/block, channels 0..7
//                  int bits: 3 per channel packed (tx_end at 3*ch)
//                  Default WS2812 channel: 4 (above FAS cha//                  ESP32-S3:      chnconf0[N] with _n suffix, 48 items/block, TX ch 0..3
//                  requires conf_update_n strobe after R/W shadow registers
//                  int bits: tx_end at ch, err at ch+4, thr at ch+8
//                  Default WS2812 channel: 0 (waveshare_ledarray has no motors)

#include "soc/rmt_struct.h"
#include "soc/gpio_sig_map.h"
#include "driver/periph_ctrl.h"
#include "driver/gpio.h"
#include "esp32-hal-gpio.h"

namespace Ws2812Rmt
{
    static const char *TAG = "Ws2812Rmt";

    // 80 MHz APB / div_cnt=2 -> 40 MHz tick -> 25 ns per tick.
    static const uint16_t T0H = 16; // 0.40 us
    static const uint16_t T0L = 34; // 0.85 us
    static const uint16_t T1H = 32; // 0.80 us
    static const uint16_t T1L = 18; // 0.45 us

    static bool s_initialised = false;
    static int  s_channel     = -1;
    static int  s_pin         = -1;

    static inline uint32_t bitItem(bool one)
    {
        // RMT item layout (bit-packed): duration0[14:0], level0[15],
        // duration1[30:16], level1[31].
        const uint16_t d0 = one ? T1H : T0H;
        const uint16_t d1 = one ? T1L : T0L;
        return ((uint32_t)d1 << 16) | (uint32_t)d0 | (1u << 15); // level0=1, level1=0
    }

// ============================================================
//  ESP32-S3 path
// ============================================================
#if defined(CONFIG_IDF_TARGET_ESP32S3)

    // S3: 4 dedicated TX channels (0..3), 48 items per block in RMTMEM.
    // CHUNK_BYTES=5 -> 40 items + 1 end marker = 41 <= 48. Safe.
    static const int CHUNK_BYTES = 5;

    static inline volatile uint32_t *channelMem(int ch)
    {
        return (volatile uint32_t *)RMTMEM.chan[ch].data32;
    }

    // S3 interrupt bit layout: tx_end=bit(ch), err=bit(ch+4), thr=bit(ch+8)
    static inline uint32_t endBitMask(int ch) { return 1u << ch; }
    static inline uint32_t chanAllMask(int ch)
    {
        return (1u << ch) | (1u << (ch + 4)) | (1u << (ch + 8));
    }

    bool begin(int gpioPin, int rmtChannel)
    {
        if (s_initialised) return true;
        if (rmtChannel < 0 || rmtChannel > 3) {
            log_e("%s: invalid S3 TX channel %d (must be 0..3)", TAG, rmtChannel);
            return false;
        }
        s_pin     = gpioPin;
        s_channel = rmtChannel;

        // periph_module_enable is ref-counted: first call enables the clock.
        periph_module_enable(PERIPH_RMT_MODULE);

        // Global S3 settings (harmless if FAS already set the same values).
        // apb_fifo_mask=1: use memory bus for RMTMEM (not the APB FIFO path).
        // sclk_sel=1:      source clock = APB (80 MHz).
        RMT.sys_conf.apb_fifo_mask = 1;
        RMT.sys_conf.sclk_sel      = 1;

        // Mask our channel's interrupts so FAS's shared ISR ignores our events.
        const uint32_t mask = chanAllMask(s_channel);
        RMT.int_ena.val &= ~mask;
        RMT.int_clr.val  =  mask;

        // Channel config -- all R/W shadow registers; commit with conf_update_n.
        RMT.chnconf0[s_channel].div_cnt_n       = 2;  // 40 MHz tick
        RMT.chnconf0[s_channel].mem_size_n      = 1;  // 48 items (CHUNK_BYTES=5 fits)
        RMT.chnconf0[s_channel].tx_conti_mode_n = 0;
        RMT.chnconf0[s_channel].idle_out_lv_n   = 0;
        RMT.chnconf0[s_channel].idle_out_en_n   = 1;
        // Commit shadow registers (S3-specific strobe).
        RMT.chnconf0[s_channel].conf_update_n   = 1;
        RMT.chnconf0[s_channel].conf_update_n   = 0;

        // Wire GPIO matrix.
        pinMode(gpioPin, OUTPUT);
        gpio_set_level((gpio_num_t)gpioPin, 0);
        gpio_matrix_out((gpio_num_t)gpioPin,
                        RMT_SIG_OUT0_IDX + s_channel, false, false);

        s_initialised = true;
        log_i("%s: ready on GPIO %d using RMT TX ch %d (ESP32-S3 bare-metal)",
              TAG, gpioPin, s_channel);
        return true;
    }

    static void fillChunk(int ch, const uint8_t *bytes, int n)
    {
        volatile uint32_t *mem = channelMem(ch);
        int slot = 0;
        for (int i = 0; i < n; ++i) {
            uint8_t b = bytes[i];
            for (int j = 7; j >= 0; --j) {
                mem[slot++] = bitItem((b >> j) & 0x1);
            }
        }
        mem[slot] = 0; // end marker -- always fits (40 items + 1 <= 48)
    }

    void show(const uint8_t *pixels, size_t numBytes)
    {
        if (!s_initialised || pixels == nullptr || numBytes == 0) return;
        log_i("%s: show() %u bytes on channel %d", TAG, (unsigned)numBytes, s_channel); 
        const int      ch     = s_channel;
        const uint32_t endBit = endBitMask(ch);

        size_t pos = 0;
        while (pos < numBytes) {
            const int chunk = (int)((numBytes - pos) > (size_t)CHUNK_BYTES
                                    ? CHUNK_BYTES : (numBytes - pos));
            fillChunk(ch, pixels + pos, chunk);

            // Reset memory read pointer (WT field -- immediate, no conf_update).
            RMT.chnconf0[ch].mem_rd_rst_n = 1;
            RMT.chnconf0[ch].mem_rd_rst_n = 0;

            // Clear stale tx_end; kick TX (tx_start_n is WT -- no conf_update).
            RMT.int_clr.val             = endBit;
            RMT.chnconf0[ch].tx_start_n = 1;

            // Poll until chunk is sent (~50 us for 40 items at 1.25 us/bit).
            uint32_t timeout = 240u * 1000u * 5u; // ~5 ms ceiling
            while (!(RMT.int_raw.val & endBit)) {
                if (--timeout == 0) {
                    log_w("%s: tx_end timeout on chunk @%u", TAG, (unsigned)pos);
                    break;
                }
            }
            RMT.int_clr.val = endBit;
            pos += chunk;
        }
        // Strip latches after >50 us of idle (idle_out_lv_n=0 keeps line low).
    }

#else // !CONFIG_IDF_TARGET_ESP32S3

// ============================================================
//  ESP32-classic path
// ============================================================
    // Classic: channels 0..7, 64 items/block, memory base 0x3FF56800.
    // Interrupt bits: 3 per channel packed, tx_end at bit (3*ch).
    // mem_size=2 -> 128 items so an 8-byte chunk (64 items) still has room
    // for the end-of-TX zero marker at slot 64.
    static const int CHUNK_BYTES = 8;

    static inline volatile uint32_t *channelMem(int ch)
    {
        // 64 items x 4 bytes = 256 bytes per block.
        return (volatile uint32_t *)(0x3FF56800 + ch * 64 * 4);
    }

    bool begin(int gpioPin, int rmtChannel)
    {
        if (s_initialised) return true;
        if (rmtChannel < 0 || rmtChannel > 7) {
            log_e("%s: invalid channel %d", TAG, rmtChannel);
            return false;
        }
        s_pin     = gpioPin;
        s_channel = rmtChannel;

        // RMT is already enabled by FAS; second call is a no-op (ref-counted).
        periph_module_enable(PERIPH_RMT_MODULE);

        // Mask our channel's interrupts -- classic: 3 bits per channel packed.
        const uint32_t chMask = (0x7u << (3 * s_channel));
        RMT.int_ena.val &= ~chMask;
        RMT.int_clr.val  =  chMask;
        

        // Per-channel config via raw registers.
        // Do NOT touch RMT.apb_conf -- FAS owns those global bits.
        RMT.conf_ch[s_channel].conf0.div_cnt      = 2;  // 40 MHz tick
        RMT.conf_ch[s_channel].conf0.mem_size      = 2;  // 128 items (borrows ch+1)
        RMT.conf_ch[s_channel].conf0.carrier_en    = 0;
        RMT.conf_ch[s_channel].conf0.mem_pd        = 0;  // power-up RAM

        RMT.conf_ch[s_channel].conf1.rx_en         = 0;
        RMT.conf_ch[s_channel].conf1.mem_owner     = 0;  // 0 = TX
        RMT.conf_ch[s_channel].conf1.tx_conti_mode = 0;
        RMT.conf_ch[s_channel].conf1.ref_always_on = 1;  // APB clock
        RMT.conf_ch[s_channel].conf1.idle_out_lv   = 0;
        RMT.conf_ch[s_channel].conf1.idle_out_en   = 1;

        // Wire GPIO matrix.
        pinMode(gpioPin, OUTPUT);
        gpio_set_level((gpio_num_t)gpioPin, 0);
        gpio_matrix_out((gpio_num_t)gpioPin,
                        RMT_SIG_OUT0_IDX + s_channel, false, false);

        s_initialised = true;
        log_i("%s: ready on GPIO %d using RMT channel %d (bare-metal, no ISR)",
              TAG, gpioPin, s_channel);
        return true;
    }

    static void fillChunk(int ch, const uint8_t *bytes, int n)
    {
        volatile uint32_t *mem = channelMem(ch);
        int slot = 0;
        for (int i = 0; i < n; ++i) {
            uint8_t b = bytes[i];
            for (int j = 7; j >= 0; --j) {
                mem[slot++] = bitItem((b >> j) & 0x1);
            }
        }
        // End marker. mem_size=2 -> 128 slots; even an 8-byte chunk (slot=64)
        // leaves slot 64..127 for the marker.
        mem[slot] = 0;
    }

    void show(const uint8_t *pixels, size_t numBytes)
    {
        if (!s_initialised || pixels == nullptr || numBytes == 0) return;
        // print for debugging 
        log_i("%s: show() %u bytes on channel %d", TAG, (unsigned)numBytes, s_channel); 
        const int      ch     = s_channel;
        const uint32_t endBit = 1u << (3 * ch); // tx_end bit for this channel

        size_t pos = 0;
        while (pos < numBytes) {
            const int chunk = (int)((numBytes - pos) > (size_t)CHUNK_BYTES
                                    ? CHUNK_BYTES : (numBytes - pos));
            fillChunk(ch, pixels + pos, chunk);

            // Reset memory read pointer to slot 0.
            RMT.conf_ch[ch].conf1.mem_rd_rst = 1;
            RMT.conf_ch[ch].conf1.mem_rd_rst = 0;

            // Clear any stale tx_end and kick TX.
            RMT.int_clr.val                  = endBit;
            RMT.conf_ch[ch].conf1.tx_start   = 1;

            // Poll until the chunk is sent (~80 us for 64 items).
            uint32_t timeout = 240u * 1000u * 5u; // ~5 ms ceiling
            while (!(RMT.int_raw.val & endBit)) {
                if (--timeout == 0) {
                    log_w("%s: tx_end timeout on chunk @%u", TAG, (unsigned)pos);
                    break;
                }
            }
            RMT.int_clr.val = endBit;
            pos += chunk;
        }
        // Strip latches after >50 us of idle (idle_out_lv=0).
    }

#endif // CONFIG_IDF_TARGET_ESP32S3

} // namespace Ws2812Rmt

#endif // ESP_PLATFORM
