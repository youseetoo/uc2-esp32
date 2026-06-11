#include "Ws2812Rmt.h"
#include <Arduino.h>
#include "esp_debug_helpers.h"
// Guard: only built on ESP32 family (classic or S3).
#ifdef ESP_PLATFORM

// Bare-metal WS2812 driver for the ESP32 RMT peripheral.
//
// Why not the IDF rmt_driver_install?
//   FastAccelStepper (gin66) owns the single shared RMT IRQ via
//   rmt_isr_register(..., ESP_INTR_FLAG_SHARED, ...). A competing
//   rmt_driver_install() would fight over that IRQ and make FAS emit garbage
//   step pulses. So we drive the strip with no IDF driver and no ISR: we fill
//   RMTMEM directly and poll for completion.
//
// Why the transmission runs with interrupts disabled:
//   A WS2812 strip latches whenever its data line idles LOW longer than the
//   reset time (~50 us). RMTMEM only holds a couple of LEDs' worth of bits, so a
//   64-LED frame must be sent as several back-to-back chunks. Refilling between
//   chunks is only a few microseconds of CPU -- but if ANY interrupt lands in
//   that window the gap stretches past the reset time and the strip latches a
//   partial frame. On this board the TWAI/CAN controller fires error interrupts
//   continuously when the master is absent, so that was happening on nearly
//   every frame -> visibly "random" colours. We therefore disable interrupts on
//   this core for the whole frame: the inter-chunk gaps stay deterministic
//   (~5 us, well under the reset time), no interrupt can preempt a refill, and
//   the strip sees one clean continuous frame. This is the same trade-off the
//   stock Adafruit NeoPixel bit-bang path makes (~2 ms with IRQs off for 64
//   LEDs). Motors are not stepping during LED commands, and deferring the CAN
//   error ISR for ~2 ms is harmless, so the cost is acceptable.
//
// Chip differences:
//   ESP32-classic: conf_ch[N].conf0/conf1, 64 items/block, channels 0..7.
//                  Default WS2812 channel: 4 (FAS owns 0..3 on UC2_4).
//   ESP32-S3:      chnconf0[N] with _n suffix + conf_update_n strobe,
//                  48 items/block, TX ch 0..3.
//                  Default WS2812 channel: 0 (waveshare_ledarray has no motors).

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

    // Guards the whole frame transmission: disables interrupts on the current
    // core so no ISR can stretch an inter-chunk gap past the WS2812 reset time.
    static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

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
        bool           timedOut = false;

        // Interrupts off for the whole frame -- see header comment. No ISR can
        // stretch an inter-chunk gap, and no refill races the TX read pointer.
        portENTER_CRITICAL(&s_mux);
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

            // Poll until the chunk is sent (~50 us for 40 items). Cycle-based
            // ceiling so it can't false-trip regardless of bus latency.
            const uint32_t t0 = ESP.getCycleCount();
            while (!(RMT.int_raw.val & endBit)) {
                if ((uint32_t)(ESP.getCycleCount() - t0) > 480000u) { // ~2 ms
                    timedOut = true;
                    break;
                }
            }
            RMT.int_clr.val = endBit;
            if (timedOut) break;
            pos += chunk;
        }
        portEXIT_CRITICAL(&s_mux);

        if (timedOut) log_w("%s: tx_end timeout (%u bytes)", TAG, (unsigned)numBytes);
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
        RMT.conf_ch[s_channel].conf0.div_cnt       = 2;  // 40 MHz tick
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
        log_i("%s: show() %u bytes on channel %d", TAG, (unsigned)numBytes, s_channel);
        const int      ch     = s_channel;
        const uint32_t endBit = 1u << (3 * ch); // tx_end bit for this channel
        bool           timedOut = false;

        // Interrupts off for the whole frame -- see header comment. No ISR can
        // stretch an inter-chunk gap, and no refill races the TX read pointer.
        portENTER_CRITICAL(&s_mux);
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

            // Poll until the chunk is sent (~80 us for 64 items). Cycle-based
            // ceiling so it can't false-trip regardless of bus latency.
            const uint32_t t0 = ESP.getCycleCount();
            while (!(RMT.int_raw.val & endBit)) {
                if ((uint32_t)(ESP.getCycleCount() - t0) > 480000u) { // ~2 ms
                    timedOut = true;
                    break;
                }
            }
            RMT.int_clr.val = endBit;
            if (timedOut) break;
            pos += chunk;
        }
        portEXIT_CRITICAL(&s_mux);

        if (timedOut) log_w("%s: tx_end timeout (%u bytes)", TAG, (unsigned)numBytes);
        // Strip latches after >50 us of idle (idle_out_lv=0 keeps line low).
    }

#endif // CONFIG_IDF_TARGET_ESP32S3

} // namespace Ws2812Rmt

#endif // ESP_PLATFORM
