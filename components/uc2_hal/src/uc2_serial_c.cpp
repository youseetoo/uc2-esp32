// T4.1: C shim over the C++ uc2::Serial_ (uc2_serial.h). See uc2_serial_c.h
// for the API. This file is C++ (it includes the C++ header and calls the
// C++ `Serial` global); it exposes an extern "C" API for the pure-C modules.
#include "uc2_hal/uc2_serial_c.h"
#include "uc2_hal/uc2_serial.h"  // the C++ uc2::Serial_ + the `Serial` global

size_t uc2_serial_write_bytes(const uint8_t *data, size_t length)
{
    if (data == NULL || length == 0)
        return 0;
    return Serial.write(data, length);
}

void uc2_serial_flush(void)
{
    Serial.flush();
}

void uc2_serial_set_timeout(uint32_t ms)
{
    Serial.setTimeout(ms);
}

int uc2_serial_available(void)
{
    return Serial.available();
}

int uc2_serial_read(void)
{
    return Serial.read();
}
