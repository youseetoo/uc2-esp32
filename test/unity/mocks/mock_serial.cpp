#include "mock_serial.h"

std::vector<uint8_t> MockSerial::receivedData;
std::vector<uint8_t> MockSerial::sentData;

void MockSerial::addBytes(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        receivedData.push_back(data[i]);
    }
}

void MockSerial::addString(const char* s) {
    if (s) {
        while (*s) {
            receivedData.push_back(*s++);
        }
    }
}

void MockSerial::clear() {
    receivedData.clear();
    sentData.clear();
}

std::string MockSerial::getLastMessage() {
    return std::string(reinterpret_cast<const char*>(receivedData.data()),
                       receivedData.size());
}

size_t MockSerial::available() {
    return receivedData.size();
}

int MockSerial::read() {
    if (receivedData.empty()) return -1;
    uint8_t b = receivedData.front();
    receivedData.erase(receivedData.begin());
    return b;
}

int MockSerial::peek() {
    if (receivedData.empty()) return -1;
    return receivedData.front();
}

size_t MockSerial::write(uint8_t b) {
    sentData.push_back(b);
    return 1;
}

void MockSerial::flush() {
    // No-op for mock
}
