#pragma once
// T5.4: minimal Wire.h shim for the M5Dial vendored MFRC522 leaf (MFRC522.{h,cpp}
// #include <Wire.h>). The M5 port comments out every Arduino Wire call (it drives
// the NFC chip through M5's own I2C instead), so this only needs to satisfy the
// include + provide a `Wire` symbol. All methods are no-ops: the RFID path is
// never used on the dial. See the sibling Arduino.h shim for why this lives here.
#include <uc2_hal.h>
#include <stdint.h>

class TwoWire {
public:
    void begin() {}
    void begin(uint8_t) {}
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission(bool = false) { return 0; }
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t *, size_t n = 0) { return n; }
    int requestFrom(uint8_t, int) { return 0; }
    int read() { return -1; }
    int available() { return 0; }
    void end() {}
    void setClock(uint32_t) {}
};
inline TwoWire Wire;
