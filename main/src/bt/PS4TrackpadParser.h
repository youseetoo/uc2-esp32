#pragma once

#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "Arduino.h"
#include "HidGamePad.h"
#include "BtController.h"

namespace BtController {

// ---------------------------------------------------------------------------
// PS4/DualShock4 trackpad parser - Bluetooth HID "extended" report
// ---------------------------------------------------------------------------
//
// The DS4 connects to the ESP32 as a standard classic-BT HID device. In the
// mode the controller starts in, it only sends the small 9-byte input report
// (HID report id 0x01: sticks, buttons, triggers) - NO touchpad data.
//
// The full 77-byte input report (HID report id 0x11) contains everything -
// sticks, buttons, triggers, motion sensors and up to 4 trackpad finger
// packets - but the controller only starts sending it after the host issues
// a GET of FEATURE report 0x02 (37 bytes). A plain feature GET flips the
// controller into "extended" mode; nothing has to be written.
// (Writing to report 0x11, as in the USB/native protocol, does NOT work over
// classic BT HID, and the feature-GET must not be issued from the
// esp_hidh callback context - see ds4PollExtendedMode() in HidController.cpp
// for where it is triggered from instead.)
//
// The 77-byte report is CRC32 protected. The controller computes
//     crc = ~CRC32( {0xA1,0x11} ++ data[0..72] )   (poly 0xEDB88320, init 0xFFFFFFFF)
// and stores it little-endian in data[73..76]. {0xA1,0x11} is the HIDP
// packet header (input report, id 0x11) as seen on the L2CAP channel.
//
// Extended report layout (offsets into the 77-byte data; the report id byte
// is stripped by the HID host stack before the event is delivered):
//   [0]      0xC0 (constant)         [34] trackpad packet count (0..4)
//   [1]      0x00 (constant)         [35..70] 4 x TrackpadPacket
//   [2..5]   LeftX LeftY RightX RightY    (0..255, 128 = centred)
//   [6..8]   buttons (PS4Buttons, 3 bytes)
//   [9][10]  LT, RT (0..255)         [71..72] padding
//   [11..12] timestamp               [73..76] crc32
//   [13]     battery (low nibble = level, bit 4 = charging)
//   [14..19] gyroscope x/y/z (int16 LE)
//   [20..25] accelerometer x/y/z (int16 LE)
//   [26..33] padding / vendor bytes
//
class PS4TrackpadParser {
public:
    // Trackpad resolution (PS4 touchpad is 1920x943 pixels)
    static const uint16_t TRACKPAD_MAX_X = 1920;
    static const uint16_t TRACKPAD_MAX_Y = 943;

    // Offsets into the 77-byte extended report
    static const uint8_t TRACKPAD_COUNT_OFFSET   = 34;
    static const uint8_t TRACKPAD_PACKETS_OFFSET = 35;
    static const uint8_t CRC_OFFSET              = 73;

    // 32-bit finger word layout (inside each TrackpadPacket). Two plausible
    // packings are selectable:
    //   0 (default): bits [2:0]=id   [14:3]=x    [26:15]=y    [27]=active
    //   1:           bit  [0]=active [3:1]=id    [15:4]=x     [27:16]=y
    //
    // Bring-up aid: the first few non-zero finger words are logged raw
    // ("trackpad raw: ..."). If the decoded coordinates look garbled, or
    // the "decoded touch out of range" warning shows up, flip this define
    // and re-flash - the raw-word log tells you which packing the
    // controller actually uses.
#ifndef DS4_FINGER_LAYOUT
#define DS4_FINGER_LAYOUT 0
#endif

    // Decode one 32-bit finger word into a TouchData
    static void decodeFingerWord(uint32_t word, TouchData *out)
    {
        out->isActive = false;
        out->id = 0;
        out->x = 0;
        out->y = 0;

#if (DS4_FINGER_LAYOUT == 1)
        out->isActive = (word & 0x1) != 0;
        out->id = (word >> 1) & 0x7;
        out->x = (word >> 4) & 0xFFF;
        out->y = (word >> 16) & 0xFFF;
#else
        out->isActive = (word >> 27) & 0x1;
        out->id = word & 0x7;
        out->x = (word >> 3) & 0xFFF;
        out->y = (word >> 15) & 0xFFF;
#endif
    }

    // Validate the CRC32 of a 77-byte extended report (data, no report id)
    static bool validateCrc(const uint8_t *data)
    {
        if (data == NULL)
            return false;

        uint8_t hdr[2] = { 0xA1, 0x11 }; // HIDP input-report header, id 0x11
        uint32_t crc = crc32(0xFFFFFFFFu, hdr, 2);
        crc = ~crc32(crc, data, CRC_OFFSET);

        uint32_t stored = (uint32_t)data[CRC_OFFSET]
                        | ((uint32_t)data[CRC_OFFSET + 1] << 8)
                        | ((uint32_t)data[CRC_OFFSET + 2] << 16)
                        | ((uint32_t)data[CRC_OFFSET + 3] << 24);
        return crc == stored;
    }

    // Parse the trackpad data from one extended report.
    // Returns RAW pixel coordinates (0..1919 / 0..943); apply
    // normalizeCoordinates() in your callback if you want 0..1000.
    static TrackpadData parseTrackpadData(const DS4DataExt *ext)
    {
        TrackpadData td = {};
        if (ext == NULL)
            return td;

        uint8_t count = ext->TrackpadPacketCount;
        if (count > 4)
            count = 4;

        // The newest packet (highest counter) carries the current touch state
        int latest = -1;
        for (uint8_t i = 0; i < count; i++) {
            if (latest < 0 || ext->Packet[i].PacketCounter > ext->Packet[latest].PacketCounter)
                latest = (int)i;
        }

        if (latest >= 0) {
            td.reportCounter = ext->Packet[latest].PacketCounter;
            decodeFingerWord(ext->Packet[latest].Finger1Data, &td.touch1);
            decodeFingerWord(ext->Packet[latest].Finger2Data, &td.touch2);
        }

        logFingerWords(ext, count, &td);
        return td;
    }

    // Normalize coordinates to a more manageable range (0-1000)
    static void normalizeCoordinates(TouchData& touch)
    {
        if (touch.isActive) {
            touch.x = (touch.x * 1000) / TRACKPAD_MAX_X;
            touch.y = (touch.y * 1000) / TRACKPAD_MAX_Y;
        }
    }

private:
    static const uint32_t *crcTable()
    {
        static uint32_t table[256];
        static bool inited = false;
        if (!inited) {
            for (uint32_t i = 0; i < 256; i++) {
                uint32_t r = i;
                for (int b = 0; b < 8; b++)
                    r = (r & 1) ? (r >> 1) ^ 0xEDB88320u : (r >> 1);
                table[i] = r;
            }
            inited = true;
        }
        return table;
    }

    static uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len)
    {
        const uint32_t *t = crcTable();
        for (size_t i = 0; i < len; i++)
            crc = t[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
        return crc;
    }

    // Bring-up logging + layout self-check
    static void logFingerWords(const DS4DataExt *ext, uint8_t count, const TrackpadData *td)
    {
        static const char *tag = "PS4Trackpad";
        static uint32_t s_logged = 0;
        static uint32_t s_lastRangeWarn = 0;

        // Log the raw finger words of the first few touches so the packing
        // can be verified (or DS4_FINGER_LAYOUT switched) from the serial log
        for (uint8_t i = 0; i < count; i++) {
            uint32_t f1 = ext->Packet[i].Finger1Data;
            uint32_t f2 = ext->Packet[i].Finger2Data;
            if (f1 != 0 || f2 != 0) {
                if (s_logged < 5)
                    ESP_LOGI(tag, "trackpad raw: pkt[%u] ctr=%u f1=0x%08lX f2=0x%08lX",
                             (unsigned)i, (unsigned)ext->Packet[i].PacketCounter, f1, f2);
                s_logged++;
                break;
            }
        }

        // A finger decoding out of the physical touchpad range is a strong
        // hint that DS4_FINGER_LAYOUT is the wrong packing
        bool oob = false;
        if (td->touch1.isActive && (td->touch1.x > TRACKPAD_MAX_X || td->touch1.y > TRACKPAD_MAX_Y))
            oob = true;
        if (td->touch2.isActive && (td->touch2.x > TRACKPAD_MAX_X || td->touch2.y > TRACKPAD_MAX_Y))
            oob = true;
        if (oob) {
            uint32_t now = millis();
            if (now - s_lastRangeWarn > 2000) {
                s_lastRangeWarn = now;
                ESP_LOGW(tag, "decoded touch out of range (t1: %u,%u t2: %u,%u) - check DS4_FINGER_LAYOUT",
                         td->touch1.x, td->touch1.y, td->touch2.x, td->touch2.y);
            }
        }
    }
};

} // namespace BtController
