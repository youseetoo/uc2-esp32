// Host stub: Preferences.h — functional in-memory key-value store.
// NVSConfig tests rely on real put*/get* semantics (defaults on miss,
// typed round-trips), so this is a working stub, not a no-op.
#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <map>

class Preferences {
public:
    Preferences() {}

    bool begin(const char* name, bool readOnly = false);
    void end();

    // Strings
    bool putString(const char* key, const char* value);
    bool getString(const char* key, char* value, size_t* length = nullptr);
    bool getString(const char* key, std::string& value, size_t* length = nullptr);
    size_t getLength(const char* key);

    // Bytes
    bool putBytes(const char* key, const void* buffer, size_t length);
    bool getBytes(const char* key, void* buffer, size_t length, size_t* return_length = nullptr);
    size_t getBytesLength(const char* key);

    // Numbers
    bool putUChar(const char* key, uint8_t value);
    bool getUChar(const char* key, uint8_t defaultValue = 0);
    bool putChar(const char* key, int8_t value);
    bool getChar(const char* key, int8_t defaultValue = 0);
    bool putShort(const char* key, int16_t value);
    bool getShort(const char* key, int16_t defaultValue = 0);
    bool putUShort(const char* key, uint16_t value);
    bool getUShort(const char* key, uint16_t defaultValue = 0);
    bool putInt(const char* key, int32_t value);
    bool getInt(const char* key, int32_t defaultValue = 0);
    bool putUInt(const char* key, uint32_t value);
    bool getUInt(const char* key, uint32_t defaultValue = 0);
    bool putFloat(const char* key, float value);
    bool getFloat(const char* key, float defaultValue = 0.0f);
    bool putDouble(const char* key, double value);
    bool getDouble(const char* key, double defaultValue = 0.0);
    bool putBool(const char* key, bool value);
    bool getBool(const char* key, bool defaultValue = false);

    bool isKey(const char* key);
    bool remove(const char* key);
    bool clear();
    int getNamespaceCount(bool readOnly);
    int getKeyCount();

    // Test hook: wipe the whole backing store (all namespaces).
    static void testClearAll();

private:
    static std::map<std::string, std::map<std::string, std::string>>& backing();

    bool open_ = false;
    bool read_only_ = false;
    std::string ns_;

    bool getRaw(const char* key, std::string& out) const;
    bool putRaw(const char* key, const std::string& v);
};
