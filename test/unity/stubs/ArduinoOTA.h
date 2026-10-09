// Host stub: ArduinoOTA.h — no-op OTA server surface.
#pragma once
#include <cstdint>
#include <functional>

enum ota_error_t {
    OTA_AUTH_ERROR = 1,
    OTA_BEGIN_ERROR,
    OTA_CONNECT_ERROR,
    OTA_RECEIVE_ERROR,
    OTA_END_ERROR
};

// Command target for getCommand(): U_FLASH (sketch) vs U_FS (filesystem).
enum { U_FLASH = 0, U_FS = 1 };

class ArduinoOTAClass {
public:
    void onStart(std::function<void()>) {}
    void onEnd(std::function<void()>) {}
    void onProgress(std::function<void(unsigned int, unsigned int)>) {}
    void onError(std::function<void(ota_error_t)>) {}
    void begin() {}
    void end() {}
    void handle() {}
    int getCommand() { return U_FLASH; }
};

extern ArduinoOTAClass ArduinoOTA;
