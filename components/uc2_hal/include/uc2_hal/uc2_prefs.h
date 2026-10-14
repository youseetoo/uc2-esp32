// T1.3 (no Arduino): uc2::Preferences — thin C++ façade over ESP-IDF NVS.
// API surface = exactly what main/ uses (grep-verified 2026-10-11,
// 237 sites / 23 files): begin/end/clear, put/get {Bool,Int,Long,Float,Char,
// UChar,UShort}, put/getString, put/getBytes + getBytesLength.
// Semantics follow Arduino-ESP32 Preferences (drop-in for module conversion):
//   * put* commits immediately (nvs_set_* + nvs_commit)
//   * get* returns defaultValue when the key is absent
//   * clear() erases every key of the namespace; the instance stays usable
//   * multiple instances / handles on one namespace are fine (NVS handles are
//     independent, like Arduino's per-instance namespaces)
//   * long = int32, same as Arduino-ESP32 (nvs i32)
// getString reads into a 512-byte stack buffer; main/ only stores short
// strings (MAC address, small values) — larger strings fall back to the default.
#pragma once

#include <cstddef>
#include <cstdint>

#include <nvs.h>

#include "uc2_hal/uc2_string.h"

namespace uc2 {

class Preferences
{
public:
    Preferences() = default;
    ~Preferences() { end(); }
    // An NVS handle is a resource: no copies.
    Preferences(const Preferences &) = delete;
    Preferences &operator=(const Preferences &) = delete;

    bool begin(const char *name, bool readOnly = false)
    {
        if (handle_ != 0) return false;
        nvs_handle_t h = 0;
        // IDF 6.1: nvs_open takes the handle as an out parameter.
        if (nvs_open(name, readOnly ? NVS_READONLY : NVS_READWRITE, &h) != ESP_OK)
        {
            return false;
        }
        handle_ = h;
        return true;
    }

    void end()
    {
        if (handle_ != 0)
        {
            nvs_close(handle_);
            handle_ = 0;
        }
    }

    // Erase all keys of the namespace (instance stays open/usable).
    // IDF 6.1: nvs_erase_all (nvs_get_next_key/nvs_erase_namespace are gone).
    void clear()
    {
        if (handle_ == 0) return;
        nvs_erase_all(handle_);
        nvs_commit(handle_);
    }

    // Remove a single key (Arduino Preferences::remove).
    void remove(const char *key)
    {
        if (handle_ == 0) return;
        nvs_erase_key(handle_, key);
        commit();
    }

    // ── bool ───────────────────────────────────────────────────────────────
    void putBool(const char *key, bool value)
    {
        nvs_set_u8(handle_, key, value ? 1 : 0);
        commit();
    }
    bool getBool(const char *key, bool defaultValue = false)
    {
        uint8_t v = defaultValue ? 1 : 0;
        if (nvs_get_u8(handle_, key, &v) != ESP_OK) return defaultValue;
        return v != 0;
    }

    // ── int (i32) ──────────────────────────────────────────────────────────
    void putInt(const char *key, int value)
    {
        nvs_set_i32(handle_, key, value);
        commit();
    }
    int getInt(const char *key, int defaultValue = 0)
    {
        int32_t v = defaultValue;
        if (nvs_get_i32(handle_, key, &v) != ESP_OK) return defaultValue;
        return v;
    }

    // ── long (i32, Arduino-compatible) ────────────────────────────────────
    void putLong(const char *key, long value)
    {
        nvs_set_i32(handle_, key, (int32_t)value);
        commit();
    }
    long getLong(const char *key, long defaultValue = 0)
    {
        int32_t v = (int32_t)defaultValue;
        if (nvs_get_i32(handle_, key, &v) != ESP_OK) return defaultValue;
        return v;
    }

    // ── float (stored as f64, like Arduino) ───────────────────────────────
    void putFloat(const char *key, float value)
    {
        nvs_set_float(handle_, key, value);
        commit();
    }
    float getFloat(const char *key, float defaultValue = 0.0f)
    {
        float v = defaultValue;
        if (nvs_get_float(handle_, key, &v) != ESP_OK) return defaultValue;
        return v;
    }

    // ── char (i8) ──────────────────────────────────────────────────────────
    void putChar(const char *key, char value)
    {
        nvs_set_i8(handle_, key, value);
        commit();
    }
    char getChar(const char *key, char defaultValue = 0)
    {
        int8_t v = defaultValue;
        if (nvs_get_i8(handle_, key, &v) != ESP_OK) return defaultValue;
        return v;
    }

    // ── uint8_t ────────────────────────────────────────────────────────────
    void putUChar(const char *key, uint8_t value)
    {
        nvs_set_u8(handle_, key, value);
        commit();
    }
    uint8_t getUChar(const char *key, uint8_t defaultValue = 0)
    {
        uint8_t v = defaultValue;
        if (nvs_get_u8(handle_, key, &v) != ESP_OK) return defaultValue;
        return v;
    }

    // ── uint16_t ───────────────────────────────────────────────────────────
    void putUShort(const char *key, uint16_t value)
    {
        nvs_set_u16(handle_, key, value);
        commit();
    }
    uint16_t getUShort(const char *key, uint16_t defaultValue = 0)
    {
        uint16_t v = defaultValue;
        if (nvs_get_u16(handle_, key, &v) != ESP_OK) return defaultValue;
        return v;
    }

    // ── string ─────────────────────────────────────────────────────────────
    void putString(const char *key, const char *value)
    {
        nvs_set_str(handle_, key, value);
        commit();
    }
    void putString(const char *key, const String &value) { putString(key, value.c_str()); }
    String getString(const char *key, const char *defaultValue = "")
    {
        char buf[512];
        size_t len = sizeof(buf);
        if (nvs_get_str(handle_, key, buf, &len) == ESP_OK) return String(buf);
        return String(defaultValue);
    }
    String getString(const char *key, const String &defaultValue)
    {
        return getString(key, defaultValue.c_str());
    }

    // ── bytes (blob, max 4000) ─────────────────────────────────────────────
    // Returns the number of bytes written (bufferSize on success, 0 on error)
    // — main/ (AxisCalibration) assigns the result, unlike Arduino's void.
    size_t putBytes(const char *key, const void *buffer, size_t bufferSize)
    {
        if (nvs_set_blob(handle_, key, buffer, bufferSize) != ESP_OK) return 0;
        nvs_commit(handle_);
        return bufferSize;
    }
    // Returns the number of bytes copied (0 if the key is absent or the
    // buffer is too small), like Arduino.
    size_t getBytes(const char *key, void *buffer, size_t bufferSize,
                    size_t bufferLen = 0)
    {
        size_t len = (bufferLen != 0) ? bufferLen : bufferSize;
        if (nvs_get_blob(handle_, key, buffer, &len) != ESP_OK) return 0;
        return len;
    }
    // Length of the stored blob (0 if the key is absent). IDF 6.1 supports
    // the NULL-buffer length query (nvs_get_blob → *length = stored size).
    size_t getBytesLength(const char *key)
    {
        size_t len = 0;
        if (nvs_get_blob(handle_, key, nullptr, &len) != ESP_OK) return 0;
        return len;
    }

private:
    nvs_handle_t handle_ = 0;

    void commit()
    {
        if (handle_ != 0) nvs_commit(handle_);
    }
};

}  // namespace uc2
