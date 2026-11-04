// PelcoParser.c — sliding-window parser for Pelco-D/P frames.
// T4.4 4i: C-ified (was PelcoParser.cpp, namespace Pelco).
#include "PelcoParser.h"
#include <string.h>

// A frame is ~7ms at 9600 baud. Anything quieter than 50ms between bytes
// means the buffer holds a stale partial frame.
#define K_GAP_RESET_MS 50

void pelco_parser_init(pelco_parser_t* p)
{
    memset(p->buf, 0, sizeof(p->buf));
    p->len = 0;
    p->lastMs = 0;
    p->bytesIn = 0;
    p->framesOk = 0;
    p->resyncs = 0;
    p->gapResets = 0;
}

static bool valid_d(const uint8_t* b, uint8_t len)
{
    if (len < 7 || b[0] != 0xFF) return false;
    uint8_t sum = 0;
    for (int i = 1; i <= 5; ++i) sum += b[i];
    return sum == b[6];
}

static bool valid_p(const uint8_t* b, uint8_t len)
{
    if (len < 8 || b[0] != 0xA0 || b[6] != 0xAF) return false;
    uint8_t x = 0;
    for (int i = 0; i <= 6; ++i) x ^= b[i];
    return x == b[7];
}

static bool try_decode(pelco_parser_t* p, pelco_frame_t* out)
{
    if (valid_d(p->buf, p->len)) {
        out->proto  = PELOCO_PROTO_D;
        out->addr   = p->buf[1];
        out->cmd1   = p->buf[2];
        out->cmd2   = p->buf[3];
        out->data1  = p->buf[4];
        out->data2  = p->buf[5];
        out->rawLen = 7;
        memcpy(out->raw, p->buf, 7);
        p->framesOk++;
        p->len = 0;
        return true;
    }
    if (valid_p(p->buf, p->len)) {
        out->proto  = PELOCO_PROTO_P;
        out->addr   = p->buf[1];
        out->cmd1   = p->buf[2];
        out->cmd2   = p->buf[3];
        out->data1  = p->buf[4];
        out->data2  = p->buf[5];
        out->rawLen = 8;
        memcpy(out->raw, p->buf, 8);
        p->framesOk++;
        p->len = 0;
        return true;
    }
    return false;
}

bool pelco_parser_feed(pelco_parser_t* p, uint8_t b, uint32_t nowMs, pelco_frame_t* out)
{
    p->bytesIn++;
    if (p->len > 0 && (uint32_t)(nowMs - p->lastMs) > K_GAP_RESET_MS) {
        p->len = 0;
        p->gapResets++;
    }
    p->lastMs = nowMs;

    // Only accept a plausible header byte at position 0.
    if (p->len == 0 && b != 0xFF && b != 0xA0) {
        p->resyncs++;
        return false;
    }

    if (p->len >= sizeof(p->buf)) {
        memmove(p->buf, p->buf + 1, sizeof(p->buf) - 1);
        p->len = sizeof(p->buf) - 1;
        p->resyncs++;
    }
    p->buf[p->len++] = b;

    while (p->len > 0) {
        if (try_decode(p, out)) return true;
        if (p->len >= sizeof(p->buf)) {
            memmove(p->buf, p->buf + 1, sizeof(p->buf) - 1);
            p->len--;
            p->resyncs++;
            continue;
        }
        break;
    }
    return false;
}

pelco_action_t pelco_decode(const pelco_frame_t* f)
{
    pelco_action_t a = {0};
    if (f->proto == PELOCO_PROTO_NONE) return a;

    // Extended opcode: cmd2 bit0 set.
    if (f->cmd2 & 0x01) {
        switch (f->cmd2) {
            case 0x03: a.ext = PELOCO_EXT_PRESET_SET;   a.extArg = f->data2; break;
            case 0x05: a.ext = PELOCO_EXT_PRESET_CLEAR; a.extArg = f->data2; break;
            case 0x07: a.ext = PELOCO_EXT_PRESET_CALL;  a.extArg = f->data2; break;
            case 0x09: a.ext = PELOCO_EXT_AUX_ON;       a.extArg = f->data2; break;
            case 0x0B: a.ext = PELOCO_EXT_AUX_OFF;      a.extArg = f->data2; break;
            default:   a.ext = PELOCO_EXT_UNKNOWN;      a.extArg = f->cmd2;  break;
        }
        return a;
    }

    // Standard motion frame.
    if ((f->cmd1 & 0x1F) == 0 && f->cmd2 == 0) {
        a.stop = true;
        return a;
    }
    if (f->cmd2 & 0x02) a.panDir  = +1;
    if (f->cmd2 & 0x04) a.panDir  = -1;
    if (f->cmd2 & 0x08) a.tiltDir = +1;
    if (f->cmd2 & 0x10) a.tiltDir = -1;
    if (f->cmd2 & 0x20) a.zoomDir = +1;
    if (f->cmd2 & 0x40) a.zoomDir = -1;
    if (f->cmd2 & 0x80) a.focusDir = +1;
    if (f->cmd1 & 0x01) a.focusDir = -1;
    if (f->cmd1 & 0x02) a.irisDir  = +1;
    if (f->cmd1 & 0x04) a.irisDir  = -1;
    a.autoScan    = (f->cmd1 & 0x10) != 0;
    a.cameraOnOff = (f->cmd1 & 0x08) != 0;

    a.panSpeed  = (f->data1 > 0x3F) ? 0x3F : f->data1;
    a.tiltSpeed = (f->data2 > 0x3F) ? 0x3F : f->data2;
    return a;
}