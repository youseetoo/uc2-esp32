#include "PelcoParser.h"
#include <string.h>

namespace Pelco
{
    // A frame is "in flight" for ~7 ms at 9600 baud. Anything quieter than
    // 50 ms between bytes means the buffer holds a stale partial frame.
    static constexpr uint32_t kGapResetMs = 50;

    static bool validD(const uint8_t* b, uint8_t len)
    {
        if (len < 7 || b[0] != 0xFF) return false;
        uint8_t sum = 0;
        for (int i = 1; i <= 5; ++i) sum += b[i];
        return sum == b[6];
    }

    static bool validP(const uint8_t* b, uint8_t len)
    {
        if (len < 8 || b[0] != 0xA0 || b[6] != 0xAF) return false;
        uint8_t x = 0;
        for (int i = 0; i <= 6; ++i) x ^= b[i];
        return x == b[7];
    }

    bool Parser::tryDecode(Frame& out)
    {
        if (validD(buf_, len_)) {
            out.proto  = Proto::D;
            out.addr   = buf_[1];
            out.cmd1   = buf_[2];
            out.cmd2   = buf_[3];
            out.data1  = buf_[4];
            out.data2  = buf_[5];
            out.rawLen = 7;
            memcpy(out.raw, buf_, 7);
            framesOk++;
            len_ = 0;
            return true;
        }
        if (validP(buf_, len_)) {
            out.proto  = Proto::P;
            out.addr   = buf_[1];
            out.cmd1   = buf_[2];
            out.cmd2   = buf_[3];
            out.data1  = buf_[4];
            out.data2  = buf_[5];
            out.rawLen = 8;
            memcpy(out.raw, buf_, 8);
            framesOk++;
            len_ = 0;
            return true;
        }
        return false;
    }

    bool Parser::feed(uint8_t b, uint32_t nowMs, Frame& out)
    {
        bytesIn++;
        if (len_ > 0 && (uint32_t)(nowMs - lastMs_) > kGapResetMs) {
            len_ = 0;
            gapResets++;
        }
        lastMs_ = nowMs;

        // Only accept a plausible header byte at position 0 — everything
        // else is inter-frame garbage.
        if (len_ == 0 && b != 0xFF && b != 0xA0) {
            resyncs++;
            return false;
        }

        if (len_ >= sizeof(buf_)) {
            memmove(buf_, buf_ + 1, sizeof(buf_) - 1);
            len_ = sizeof(buf_) - 1;
            resyncs++;
        }
        buf_[len_++] = b;

        while (len_ > 0) {
            if (tryDecode(out)) return true;
            // A full buffer that still doesn't validate: drop the head byte
            // and retry so a corrupted header can't wedge the stream.
            if (len_ >= sizeof(buf_)) {
                memmove(buf_, buf_ + 1, sizeof(buf_) - 1);
                len_--;
                resyncs++;
                continue;
            }
            break;
        }
        return false;
    }

    Action decode(const Frame& f)
    {
        Action a;
        if (f.proto == Proto::None) return a;

        // Extended opcode: cmd2 bit0 set (cmd1 is 0x00 for the ones we care
        // about; keyboards use these for preset / aux keys).
        if (f.cmd2 & 0x01) {
            switch (f.cmd2) {
                case 0x03: a.ext = Ext::PresetSet;   a.extArg = f.data2; break;
                case 0x05: a.ext = Ext::PresetClear; a.extArg = f.data2; break;
                case 0x07: a.ext = Ext::PresetCall;  a.extArg = f.data2; break;
                case 0x09: a.ext = Ext::AuxOn;       a.extArg = f.data2; break;
                case 0x0B: a.ext = Ext::AuxOff;      a.extArg = f.data2; break;
                default:   a.ext = Ext::Unknown;     a.extArg = f.cmd2;  break;
            }
            return a;
        }

        // Standard motion frame.
        if ((f.cmd1 & 0x1F) == 0 && f.cmd2 == 0) {
            a.stop = true;
            return a;
        }
        if (f.cmd2 & 0x02) a.panDir  = +1;
        if (f.cmd2 & 0x04) a.panDir  = -1;
        if (f.cmd2 & 0x08) a.tiltDir = +1;
        if (f.cmd2 & 0x10) a.tiltDir = -1;
        if (f.cmd2 & 0x20) a.zoomDir = +1;
        if (f.cmd2 & 0x40) a.zoomDir = -1;
        if (f.cmd2 & 0x80) a.focusDir = +1;
        if (f.cmd1 & 0x01) a.focusDir = -1;
        if (f.cmd1 & 0x02) a.irisDir  = +1;
        if (f.cmd1 & 0x04) a.irisDir  = -1;
        a.autoScan    = (f.cmd1 & 0x10) != 0;
        a.cameraOnOff = (f.cmd1 & 0x08) != 0;

        a.panSpeed  = (f.data1 > 0x3F) ? 0x3F : f.data1;
        a.tiltSpeed = (f.data2 > 0x3F) ? 0x3F : f.data2;
        return a;
    }
}
