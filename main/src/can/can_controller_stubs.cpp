/**
 * @file can_controller_stubs.cpp
 * @brief No-op stubs for can_controller:: functions when CANopen replaces old CAN transport.
 *
 * When CAN_CONTROLLER_CANOPEN is enabled, the old can_transport.cpp is excluded from
 * the build to avoid double-initializing the TWAI driver. However, many controller
 * files (FocusMotor, LaserController, etc.) still reference can_controller:: functions
 * under CAN_BUS_ENABLED / CAN_SEND_COMMANDS guards. These stubs satisfy the linker
 * without pulling in the old TWAI-based transport.
 */
// TODO: This is only temporary until we fully integrate CANopen and can remove the old transport layer. Once CANopen is fully integrated, we should remove these stubs and update all references to can_controller:: functions to use the new CANopen stack instead. For now, this allows us to switch between the old transport and the new CANopen stack without changing the existing structure too much.

#if defined(CAN_CONTROLLER_CANOPEN) && defined(CAN_BUS_ENABLED)

#include "can_transport.h"

namespace can_controller
{
    // Storage for extern variables declared in header
    uint8_t device_can_id = 0;
    bool debugState = false;
    uint8_t separationTimeMin = 5;

    // -- general --
    int act(cJSON *) { return 0; }
    cJSON *get(cJSON *) { return nullptr; }
    void setup() {}
    void loop() {}

    uint8_t axis2id(int) { return 0; }
    int receiveCanMessage(uint8_t) { return 0; }
    int receiveCanMessage(uint8_t *, uint8_t) { return 0; }
    int sendCanMessage(uint8_t, const uint8_t *, size_t) { return 0; }
    int sendIsoTpData(uint8_t, const uint8_t *, size_t) { return 0; }
    int sendTypedCanMessage(uint8_t, CANMessageTypeID, const uint8_t *, size_t) { return 0; }
    void setCANAddress(uint8_t) {}
    uint8_t getCANAddress() { return device_can_id; }
    void dispatchIsoTpData(pdu_t &) {}

    // -- motor --
    int sendMotorDataToCANDriver(MotorData, uint8_t, int) { return 0; }
    MotorSettings extractMotorSettings(const MotorData &m) {
        MotorSettings s = {};
        return s;
    }
    int sendMotorSettingsToCANDriver(MotorSettings, uint8_t) { return 0; }
    void resetMotorSettingsFlag(uint8_t) {}
    void resetAllMotorSettingsFlags() {}
    int startStepper(MotorData *, int, int) { return 0; }
    void stopStepper(Stepper) {}
    void sendMotorStateToMaster() {}
    void sendQidReportToMaster(int16_t, uint8_t) {}
    bool isMotorRunning(int) { return false; }
    int sendMotorSpeedToCanDriver(uint8_t, int32_t) { return 0; }
    int sendEncoderBasedMotionToCanDriver(uint8_t, bool) { return 0; }
    int sendMotorSingleValue(uint8_t, uint16_t, int32_t) { return 0; }
    int sendCANRestartByID(uint8_t) { return 0; }

    // -- scan --
    cJSON *scanCanDevices() { return nullptr; }
    bool isCANDeviceOnline(uint8_t) { return false; }
    void addCANDeviceToAvailableList(uint8_t) {}
    void removeCANDeviceFromAvailableList(uint8_t) {}

    // -- home --
    void sendHomeDataToCANDriver(HomeData, uint8_t) {}
    void sendHomeStateToMaster(HomeState) {}
    void sendStopHomeToCANDriver(uint8_t) {}

    // -- laser --
    void sendLaserDataToCANDriver(LaserData) {}

    // -- galvo --
#ifdef GALVO_CONTROLLER
    void sendGalvoDataToCANDriver(GalvoData) {}
    void sendGalvoStateToMaster(GalvoData) {}
    void sendGalvoPointsToCANDriver(const ArbitraryScanPoint *, uint16_t, TriggerMode) {}
#endif

    // -- TMC --
#ifdef TMC_CONTROLLER
    void sendTMCDataToCANDriver(TMCData, int) {}
#endif

    // -- LED --
#ifdef LED_CONTROLLER
    int sendLedCommandToCANDriver(LedCommand, uint8_t) { return 0; }
#endif

    // -- OTA --
    int sendOtaStartCommandToSlave(uint8_t, const char *, const char *, uint32_t) { return 0; }
    void handleOtaCommand(OtaWifiCredentials *) {}
    void sendOtaAck(uint8_t) {}
    void handleOtaLoop() {}
    cJSON *actCanOta(cJSON *) { return nullptr; }
    cJSON *actCanOtaStream(cJSON *) { return nullptr; }

} // namespace can_controller

#endif // CAN_CONTROLLER_CANOPEN && CAN_BUS_ENABLED
