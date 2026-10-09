#include "mock_nvs.h"

namespace MockNvs {
    std::map<std::string, std::string> store;

    void clear() {
        store.clear();
    }

    bool set(const char* namespaceName, const char* key, const void* value, size_t len) {
        std::string ns = namespaceName ? namespaceName : "";
        std::string k = key ? key : "";
        std::string val(static_cast<const char*>(value), len);
        store[ns + "/" + k] = val;
        return true;
    }

    bool get(const char* namespaceName, const char* key, void* value, size_t* len) {
        std::string ns = namespaceName ? namespaceName : "";
        std::string k = key ? key : "";
        auto it = store.find(ns + "/" + k);
        if (it == store.end()) return false;
        size_t copyLen = it->second.size();
        if (len && (size_t)*len < copyLen) copyLen = *len;
        if (value) memcpy(value, it->second.c_str(), copyLen);
        if (len) *len = copyLen;
        return true;
    }

    bool erase(const char* namespaceName) {
        if (!namespaceName) return false;
        std::string prefix = namespaceName;
        bool erased = false;
        for (auto it = store.begin(); it != store.end(); ) {
            if (it->first.find(prefix + "/") == 0) {
                it = store.erase(it);
                erased = true;
            } else {
                ++it;
            }
        }
        return erased;
    }

    bool eraseKey(const char* namespaceName, const char* key) {
        std::string ns = namespaceName ? namespaceName : "";
        std::string k = key ? key : "";
        auto it = store.find(ns + "/" + k);
        if (it == store.end()) return false;
        store.erase(it);
        return true;
    }
}
