#pragma once
#include <map>
#include <string>
#include <cstring>

// In-memory NVS simulator for unit tests.
namespace MockNvs {
    extern std::map<std::string, std::string> store;

    void clear();
    bool set(const char* namespaceName, const char* key, const void* value, size_t len);
    bool get(const char* namespaceName, const char* key, void* value, size_t* len);
    bool erase(const char* namespaceName);
    bool eraseKey(const char* namespaceName, const char* key);
}
