// PelcoParser.h — checksum-validated sliding-window parser for Pelco-D/P frames.
//
// Pelco-D  7 bytes:  FF | addr | cmd1 | cmd2 | data1 | data2 | sum(1..5)
// Pelco-P  8 bytes:  A0 | addr | d1   | d2   | d3    | d4    | AF | xor(0..6)
//
// Both protocols share the same command-bit semantics; decode() maps a
// Pelco-P frame onto the Pelco-D layout and uses one bit table.
//
// Parsing strategy mirrors levkovigor/PTZProtocolHandler (MIT): accumulate
// bytes, try to validate the buffer head as D then P, shift by one byte on
// overflow (resync), and reset on an inter-byte gap so a partial frame can't
// poison the next one.
//
// T4.4 4i: C-ified (was namespace Pelco).

#ifndef PELOCO_PARSER_H
#define PELOCO_PARSER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PELOCO_PROTO_NONE = 0,
    PELOCO_PROTO_D,
    PELOCO_PROTO_P
} pelco_proto_t;

typedef struct {
    pelco_proto_t proto;
    uint8_t addr;
    uint8_t cmd1;
    uint8_t cmd2;
    uint8_t data1;
    uint8_t data2;
    uint8_t raw[8];
    uint8_t rawLen;
} pelco_frame_t;

typedef enum {
    PELOCO_EXT_NONE = 0,
    PELOCO_EXT_PRESET_SET,
    PELOCO_EXT_PRESET_CLEAR,
    PELOCO_EXT_PRESET_CALL,
    PELOCO_EXT_AUX_ON,
    PELOCO_EXT_AUX_OFF,
    PELOCO_EXT_UNKNOWN
} pelco_ext_t;

typedef struct {
    bool      stop;
    int8_t    panDir;
    int8_t    tiltDir;
    uint8_t   panSpeed;
    uint8_t   tiltSpeed;
    int8_t    zoomDir;
    int8_t    focusDir;
    int8_t    irisDir;
    bool      autoScan;
    bool      cameraOnOff;
    pelco_ext_t ext;
    uint8_t   extArg;
} pelco_action_t;

// Parser state (one per UART port).
typedef struct {
    uint8_t   buf[8];
    uint8_t   len;
    uint32_t  lastMs;
    uint32_t  bytesIn;
    uint32_t  framesOk;
    uint32_t  resyncs;
    uint32_t  gapResets;
} pelco_parser_t;

void pelco_parser_init(pelco_parser_t* p);
bool pelco_parser_feed(pelco_parser_t* p, uint8_t b, uint32_t nowMs, pelco_frame_t* out);
pelco_action_t pelco_decode(const pelco_frame_t* f);

#endif // PELOCO_PARSER_H