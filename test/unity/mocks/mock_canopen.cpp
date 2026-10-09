#include "mock_canopen.h"

namespace MockCanopen {
    std::vector<MockSdoRequest> receivedWrites;
    std::vector<MockSdoRequest> receivedReads;
    bool sdoWriteResult = true;
    bool sdoReadResult = true;
    std::vector<uint8_t> sdoReadResponse;
    bool nodeReachable = true;
}

#ifdef UNIT_TEST
extern "C" {
    int CO_writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                    uint8_t* data, size_t dataSize, int timeoutMs) {
        (void)timeoutMs;
        MockCanopen::receivedWrites.push_back({nodeId, index, subIndex,
            std::vector<uint8_t>(data, data + dataSize)});
        return MockCanopen::sdoWriteResult ? 0 : 1;
    }

    int CO_readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                   uint8_t* data, size_t dataSize, int timeoutMs) {
        (void)nodeId; (void)index; (void)subIndex; (void)timeoutMs;
        MockCanopen::receivedReads.push_back({nodeId, index, subIndex, {}});
        if (!MockCanopen::sdoReadResult) return 1;
        size_t copyLen = MockCanopen::sdoReadResponse.size();
        if (copyLen > dataSize) copyLen = dataSize;
        if (data && copyLen > 0) {
            memcpy(data, MockCanopen::sdoReadResponse.data(), copyLen);
        }
        return 0;
    }
}
#endif
