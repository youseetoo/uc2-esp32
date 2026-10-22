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

// Struct (not namespace): test_canopen_module.cpp calls MockCanopen::reset()
// etc. as static members.
struct MockCanopen {
    static std::vector<MockSdoRequest> receivedWrites;
    static std::vector<MockSdoRequest> receivedReads;
    static bool sdoWriteResult;
    static bool sdoReadResult;
    static std::vector<uint8_t> sdoReadResponse;
    static bool nodeReachable;

    static void reset();
    static void setWriteResult(bool result);
    static void setReadResult(bool result, const std::vector<uint8_t>& data);
    static void setNodeReachable(bool reachable);
};

// Override CO_writeSDO / CO_readSDO in tests
#ifdef UNIT_TEST
    #include <cstring>
    extern "C" {
        int CO_writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                        uint8_t* data, size_t dataSize, int timeoutMs);
        int CO_readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                       uint8_t* data, size_t dataSize, size_t* bytesRead,
                       int timeoutMs);
    }
#endif
