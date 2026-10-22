// Host-test doubles for the CANopen surface used by uc2_canopen.
//
// Two layers:
//  1. MockCanopen — C++ state class for the SDO C-shim (CO_writeSDO /
//     CO_readSDO / CO_nodeReachable) that CANopenModule's UNIT_TEST branches
//     route through (see the #ifdef UNIT_TEST blocks in CANopenModule.cpp).
//  2. No-op shims for the CANopenNode 301 stack functions that CANopenModule.cpp
//     references but which are not linked in the host build (the real stack
//     needs the ESP32 TWAI driver). Signatures must match the real headers
//     (301/*.h, CO_driver_target.h, OD.h) — this file includes them so the
//     compiler enforces it.
#include "mock_canopen.h"

#include <CANopen.h>           // CO_t, CO_NMT_*, CO_SDO_* prototypes (301/*.h)
#include "CO_driver_target.h"  // CO_CANinterrupt / CO_CANtx_retryQueued / CO_CANclearPendingTx
#include "OD.h"                // OD_t / OD_entry_t for OD_find
#include <cstring>
#include <cstdlib>

std::vector<MockSdoRequest> MockCanopen::receivedWrites;
std::vector<MockSdoRequest> MockCanopen::receivedReads;
bool MockCanopen::sdoWriteResult = true;
bool MockCanopen::sdoReadResult = true;
std::vector<uint8_t> MockCanopen::sdoReadResponse;
bool MockCanopen::nodeReachable = true;

void MockCanopen::reset() {
    receivedWrites.clear();
    receivedReads.clear();
    sdoWriteResult = true;
    sdoReadResult = true;
    sdoReadResponse.clear();
    nodeReachable = true;
}

void MockCanopen::setWriteResult(bool result) { sdoWriteResult = result; }

void MockCanopen::setReadResult(bool result, const std::vector<uint8_t>& data) {
    sdoReadResult = result;
    sdoReadResponse = data;
}

void MockCanopen::setNodeReachable(bool reachable) { nodeReachable = reachable; }

#ifdef UNIT_TEST
extern "C" {

// ---------------------------------------------------------------------------
// SDO C-shim transport (host mock of the remote-node SDO endpoint)
// ---------------------------------------------------------------------------
int CO_writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                uint8_t* data, size_t dataSize, int timeoutMs) {
    (void)timeoutMs;
    MockCanopen::receivedWrites.push_back({nodeId, index, subIndex,
        std::vector<uint8_t>(data, data + dataSize)});
    return MockCanopen::sdoWriteResult ? 0 : 1;
}

int CO_readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
               uint8_t* data, size_t dataSize, size_t* bytesRead, int timeoutMs) {
    (void)timeoutMs;
    MockCanopen::receivedReads.push_back({nodeId, index, subIndex, {}});
    if (!MockCanopen::sdoReadResult) {
        if (bytesRead) *bytesRead = 0;
        return 1;
    }
    size_t copyLen = MockCanopen::sdoReadResponse.size();
    if (copyLen > dataSize) copyLen = dataSize;
    if (data && copyLen > 0) {
        memcpy(data, MockCanopen::sdoReadResponse.data(), copyLen);
    }
    if (bytesRead) *bytesRead = copyLen;
    return 0;
}

bool CO_nodeReachable(uint8_t nodeId) {
    (void)nodeId;
    return MockCanopen::nodeReachable;
}

// ---------------------------------------------------------------------------
// CANopenNode 301 stack no-op shims (real stack is not linked on host).
// Bodies are inert: unit tests never drive the stack; these exist so the
// whole of CANopenModule.cpp links.
// ---------------------------------------------------------------------------
CO_t *CO_new(CO_config_t *config, uint32_t *heapMemoryUsed) {
    (void)config;
    if (heapMemoryUsed) *heapMemoryUsed = 0;
    return NULL;
}

void CO_delete(CO_t *co) { (void)co; }

CO_ReturnError_t CO_CANinit(CO_t *co, void *CANptr, uint16_t bitRate) {
    (void)co; (void)CANptr; (void)bitRate;
    return CO_ERROR_NO;
}

CO_ReturnError_t CO_LSSinit(CO_t *co, CO_LSS_address_t *lssAddress,
                            uint8_t *pendingNodeID, uint16_t *pendingBitRate) {
    (void)co; (void)lssAddress; (void)pendingNodeID; (void)pendingBitRate;
    return CO_ERROR_NO;
}

CO_ReturnError_t CO_CANopenInit(CO_t *co,
                                CO_NMT_t *NMT,
                                CO_EM_t *em,
                                OD_t *od,
                                OD_entry_t *OD_statusBits,
                                CO_NMT_control_t NMTcontrol,
                                uint16_t firstHBTime_ms,
                                uint16_t SDOserverTimeoutTime_ms,
                                uint16_t SDOclientTimeoutTime_ms,
                                bool_t SDOclientBlockTransfer,
                                uint8_t nodeId,
                                uint32_t *errInfo) {
    (void)co; (void)NMT; (void)em; (void)od; (void)OD_statusBits;
    (void)NMTcontrol; (void)firstHBTime_ms; (void)SDOserverTimeoutTime_ms;
    (void)SDOclientTimeoutTime_ms; (void)SDOclientBlockTransfer;
    (void)nodeId; (void)errInfo;
    return CO_ERROR_NO;
}

CO_ReturnError_t CO_CANopenInitPDO(CO_t *co, CO_EM_t *em, OD_t *od,
                                   uint8_t nodeId, uint32_t *errInfo) {
    (void)co; (void)em; (void)od; (void)nodeId; (void)errInfo;
    return CO_ERROR_NO;
}

CO_NMT_reset_cmd_t CO_process(CO_t *co, bool_t enableGateway,
                              uint32_t timeDifference_us,
                              uint32_t *timerNext_us) {
    (void)co; (void)enableGateway; (void)timeDifference_us; (void)timerNext_us;
    return CO_RESET_NOT;
}

bool_t CO_process_SYNC(CO_t *co, uint32_t timeDifference_us,
                       uint32_t *timerNext_us) {
    (void)co; (void)timeDifference_us; (void)timerNext_us;
    return false;
}

void CO_process_RPDO(CO_t *co, bool_t syncWas, uint32_t timeDifference_us,
                     uint32_t *timerNext_us) {
    (void)co; (void)syncWas; (void)timeDifference_us; (void)timerNext_us;
}

void CO_process_TPDO(CO_t *co, bool_t syncWas, uint32_t timeDifference_us,
                     uint32_t *timerNext_us) {
    (void)co; (void)syncWas; (void)timeDifference_us; (void)timerNext_us;
}

void CO_CANinterrupt(CO_CANmodule_t *CANmodule) { (void)CANmodule; }
void CO_CANtx_retryQueued(CO_CANmodule_t *CANmodule) { (void)CANmodule; }
void CO_CANclearPendingTx(CO_CANmodule_t *CANmodule) { (void)CANmodule; }
void CO_CANsetConfigurationMode(void *CANptr) { (void)CANptr; }
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule) { (void)CANmodule; }
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule) { (void)CANmodule; }

void CO_EM_initCallbackRx(CO_EM_t *em,
                          void (*pFunctSignalRx)(const uint16_t ident,
                                                 const uint16_t errorCode,
                                                 const uint8_t errorRegister,
                                                 const uint8_t errorBit,
                                                 const uint32_t infoCode)) {
    (void)em; (void)pFunctSignalRx;
}

void CO_error(CO_EM_t *em, bool_t setError, const uint8_t errorBit,
              uint16_t errorCode, uint32_t infoCode) {
    (void)em; (void)setError; (void)errorBit; (void)errorCode; (void)infoCode;
}

OD_entry_t *OD_find(OD_t *od, uint16_t index) {
    (void)od; (void)index;
    return NULL;
}

// SDO client: report a protocol error so any (unreachable in tests) caller
// aborts cleanly instead of looping on "in progress".
CO_SDO_return_t CO_SDOclient_setup(CO_SDOclient_t *SDO_C,
                                   uint32_t COB_IDClientToServer,
                                   uint32_t COB_IDServerToClient,
                                   uint8_t nodeIDOfTheSDOServer) {
    (void)SDO_C; (void)COB_IDClientToServer; (void)COB_IDServerToClient;
    (void)nodeIDOfTheSDOServer;
    return CO_SDO_RT_wrongArguments;
}

CO_SDO_return_t CO_SDOclientDownloadInitiate(CO_SDOclient_t *SDO_C,
                                             uint16_t index,
                                             uint8_t subIndex,
                                             size_t sizeIndicated,
                                             uint16_t SDOtimeoutTime_ms,
                                             bool_t blockEnable) {
    (void)SDO_C; (void)index; (void)subIndex; (void)sizeIndicated;
    (void)SDOtimeoutTime_ms; (void)blockEnable;
    return CO_SDO_RT_wrongArguments;
}

size_t CO_SDOclientDownloadBufWrite(CO_SDOclient_t *SDO_C,
                                    const uint8_t *buf,
                                    size_t count) {
    (void)SDO_C; (void)buf;
    return count;
}

CO_SDO_return_t CO_SDOclientDownload(CO_SDOclient_t *SDO_C,
                                     uint32_t timeDifference_us,
                                     bool_t abort,
                                     bool_t bufferPartial,
                                     CO_SDO_abortCode_t *SDOabortCode,
                                     size_t *sizeTransferred,
                                     uint32_t *timerNext_us) {
    (void)SDO_C; (void)timeDifference_us; (void)abort; (void)bufferPartial;
    (void)sizeTransferred; (void)timerNext_us;
    if (SDOabortCode) *SDOabortCode = CO_SDO_AB_GENERAL;
    return CO_SDO_RT_endedWithServerAbort;
}

CO_SDO_return_t CO_SDOclientUploadInitiate(CO_SDOclient_t *SDO_C,
                                           uint16_t index,
                                           uint8_t subIndex,
                                           uint16_t SDOtimeoutTime_ms,
                                           bool_t blockEnable) {
    (void)SDO_C; (void)index; (void)subIndex; (void)SDOtimeoutTime_ms;
    (void)blockEnable;
    return CO_SDO_RT_wrongArguments;
}

CO_SDO_return_t CO_SDOclientUpload(CO_SDOclient_t *SDO_C,
                                   uint32_t timeDifference_us,
                                   bool_t abort,
                                   CO_SDO_abortCode_t *SDOabortCode,
                                   size_t *sizeIndicated,
                                   size_t *sizeTransferred,
                                   uint32_t *timerNext_us) {
    (void)SDO_C; (void)timeDifference_us; (void)abort;
    (void)sizeIndicated; (void)sizeTransferred; (void)timerNext_us;
    if (SDOabortCode) *SDOabortCode = CO_SDO_AB_GENERAL;
    return CO_SDO_RT_endedWithServerAbort;
}

size_t CO_SDOclientUploadBufRead(CO_SDOclient_t *SDO_C,
                                 uint8_t *buf,
                                 size_t count) {
    (void)SDO_C; (void)buf; (void)count;
    return 0;
}

void CO_SDOclientClose(CO_SDOclient_t *SDO_C) { (void)SDO_C; }

} // extern "C"
#endif // UNIT_TEST
