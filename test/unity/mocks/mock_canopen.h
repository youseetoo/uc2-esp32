#pragma once
#include <cstdint>
#include <vector>

// Mock CANopenNode SDO functions for unit tests.
struct MockSdoRequest {
    uint8_t nodeId;
    uint16_t index;
    uint8_t subIndex;
    std::vector<uint8_t> data;
};

namespace MockCanopen {
    extern std::vector<MockSdoRequest> receivedWrites;
    extern std::vector<MockSdoRequest> receivedReads;
    extern bool sdoWriteResult;
    extern bool sdoReadResult;
    extern std::vector<uint8_t> sdoReadResponse;
    extern bool nodeReachable;

    void reset();
    void setWriteResult(bool result);
    void setReadResult(bool result, const std::vector<uint8_t>& data);
    void setNodeReachable(bool reachable);
}

// Override CO_writeSDO / CO_readSDO in tests
#ifdef UNIT_TEST
    #include <cstring>
    extern "C" {
        int CO_writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                        uint8_t* data, size_t dataSize, int timeoutMs);
        int CO_readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                       uint8_t* data, size_t dataSize, int timeoutMs);
    }
#endif
