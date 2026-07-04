// PelcoParser.h — checksum-validated sliding-window parser for the two wire
// protocols a CCTV PTZ keyboard emits on its RS-485 port:
//
//   Pelco-D  7 bytes:  FF | addr | cmd1 | cmd2 | data1 | data2 | sum(1..5)
//   Pelco-P  8 bytes:  A0 | addr | d1   | d2   | d3    | d4    | AF | xor(0..6)
//
// The keyboard (e.g. TPS3103C-family "Keyboard Controller Ver 3.057") is the
// bus master; we are a listening "camera". Set the keyboard to Pelco-D via
// its menu (995+AUX → PROTOCOL → PTZ → PELCO_D, or shortcut 44+ACK) and pick
// the camera address with N+CAM.
//
// Both protocols share the same command-bit semantics, so decode() maps a
// Pelco-P frame onto the Pelco-D layout and uses one bit table:
//
//   cmd1: 0x80 sense  0x10 auto-scan  0x08 camera on/off
//         0x04 iris close  0x02 iris open  0x01 focus near
//   cmd2: 0x80 focus far  0x40 zoom wide  0x20 zoom tele
//         0x10 tilt down  0x08 tilt up  0x04 pan left  0x02 pan right
//   cmd2 bit0 set → extended opcode instead of motion bits:
//         0x03 set preset  0x05 clear preset  0x07 call preset
//         0x09 aux on      0x0B aux off             (data2 = preset/aux #)
//   data1 = pan speed 0..0x3F (0xFF = turbo), data2 = tilt speed 0..0x3F
//   all-zero cmd1/cmd2 = STOP.
//
// Parsing strategy mirrors levkovigor/PTZProtocolHandler (MIT): accumulate
// bytes, try to validate the buffer head as D then P, shift by one byte on
// overflow (resync), and reset on an inter-byte gap so a partial frame can't
// poison the next one.
#pragma once
#include <stdint.h>

namespace Pelco
{
    enum class Proto : uint8_t { None = 0, D, P };

    // Raw validated frame, normalised to the Pelco-D field layout.
    struct Frame
    {
        Proto   proto  = Proto::None;
        uint8_t addr   = 0;
        uint8_t cmd1   = 0;
        uint8_t cmd2   = 0;
        uint8_t data1  = 0;
        uint8_t data2  = 0;
        uint8_t raw[8] = {0};
        uint8_t rawLen = 0;
    };

    // Semantic decode of a Frame.
    enum class Ext : uint8_t
    {
        None = 0,     // standard motion frame
        PresetSet,    // arg = preset number
        PresetClear,
        PresetCall,
        AuxOn,        // arg = aux number
        AuxOff,
        Unknown       // extended opcode we don't decode (arg = cmd2)
    };

    struct Action
    {
        bool    stop      = false; // all-zero standard frame (joystick centred / key released)
        int8_t  panDir    = 0;     // -1 left, +1 right
        int8_t  tiltDir   = 0;     // -1 down, +1 up
        uint8_t panSpeed  = 0;     // 0..63 (0xFF turbo is clamped to 63)
        uint8_t tiltSpeed = 0;
        int8_t  zoomDir   = 0;     // +1 tele (in), -1 wide (out)
        int8_t  focusDir  = 0;     // +1 far, -1 near
        int8_t  irisDir   = 0;     // +1 open, -1 close
        bool    autoScan  = false;
        bool    cameraOnOff = false;
        Ext     ext       = Ext::None;
        uint8_t extArg    = 0;
    };

    Action decode(const Frame& f);

    class Parser
    {
    public:
        // Feed one received byte; returns true when `out` holds a complete,
        // checksum-valid frame. `nowMs` drives the inter-byte gap reset.
        bool feed(uint8_t b, uint32_t nowMs, Frame& out);

        // Diagnostics (exposed via /ptz_get)
        uint32_t bytesIn        = 0;
        uint32_t framesOk       = 0;
        uint32_t resyncs        = 0;  // shifted-out bytes (garbage / wrong baud)
        uint32_t gapResets      = 0;

    private:
        uint8_t  buf_[8]  = {0};
        uint8_t  len_     = 0;
        uint32_t lastMs_  = 0;

        bool tryDecode(Frame& out);
    };
}
