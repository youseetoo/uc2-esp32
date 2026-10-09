#pragma once
#include <Arduino.h>
#include <vector>
#include <string>

// Mock Stream for unit tests — replaces Serial and other Stream-based I/O.
class MockSerial : public Stream {
public:
    void addBytes(const uint8_t* data, size_t len);
    void addString(const char* s);

    void clear();
    std::string getLastMessage();
    size_t available() override;
    int read() override;
    int peek() override;
    size_t write(uint8_t) override;
    void flush() override;

    static std::vector<uint8_t> receivedData;
    static std::vector<uint8_t> sentData;
};
