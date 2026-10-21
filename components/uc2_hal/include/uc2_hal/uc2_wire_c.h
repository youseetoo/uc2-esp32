#pragma once
// C-facing API for the uc2::Wire global (Phase 4 C conversion).
// uc2_wire.h itself stays C++ (class-based); .c modules (Tmp102, Fan, ...)
// include THIS header and call the wire_* functions below.
// Implemented in uc2_hal/src/uc2_wire.cpp, forwarding to the global Wire.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Master bus on I2C_NUM_0 (Arduino 3-arg begin).
bool    wire_begin(int sda, int scl, int freq_hz);
void    wire_begin_transmission(uint8_t addr);
size_t  wire_write(uint8_t b);
// Bulk write (Arduino Wire.write(buf, len)); returns the requested length.
size_t  wire_write_buf(const uint8_t *buf, size_t len);
// stop=false leaves the bus open (Arduino semantics; with the IDF v2 driver a
// read after this MUST go through wire_request_from / wire_write_read, not a
// second beginTransmission).
uint8_t wire_end_transmission(bool stop);
// Returns bytes received (qty on success, 0 on bus error).
int     wire_request_from(uint8_t addr, int qty);
// Bytes currently in the rx buffer (Arduino Wire.available()).
int     wire_available(void);
// Next received byte, -1 when the rx buffer is empty.
int     wire_read(void);
// Write then read within ONE transaction (repeated START).
// Returns bytes read, or -1 on error (errCode out-param gets the
// endTransmission-style code).
int     wire_write_read(uint8_t addr, const uint8_t *wr, int wr_len,
                        uint8_t *rd, int rd_len, uint8_t *err_code);
