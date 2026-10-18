// T4.1: C shim over the C++ uc2::Serial_ (uc2_serial.h) so pure-C modules
// (e.g. the serial output path in uc2_serial_out.c) can drive the UART
// without depending on the C++ façade. The shim is defined in uc2_serial_c.cpp
// (which includes the C++ header) and exposes an extern "C" API.
//
// Only the subset the C modules need is exposed:
//   uc2_serial_write_bytes — write raw bytes (Serial.write(data, len))
//   uc2_serial_flush       — wait until the TX is empty (Serial.flush())
//   uc2_serial_set_timeout — set the read timeout in ms (Serial.setTimeout)
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Write `length` bytes from `data` to the device serial port (UART0 / `Serial`).
// Returns the number of bytes actually written (0 on failure / uninstalled).
size_t uc2_serial_write_bytes(const uint8_t *data, size_t length);

// Block until everything queued for TX is out (Arduino flush semantics).
void uc2_serial_flush(void);

// Set the default read timeout in milliseconds (applied to uc2_serial_read_bytes).
void uc2_serial_set_timeout(uint32_t ms);

// Number of bytes currently buffered for RX (Serial.available()).
int uc2_serial_available(void);

// Read one byte (Serial.read()); returns the byte (0..255) or -1 when nothing
// is buffered.
int uc2_serial_read(void);

#ifdef __cplusplus
}
#endif
