/**
 * @file canopen_slave.cpp
 * @brief CANopen slave/satellite node implementation for UC2-ESP32
 */

#include "canopen_slave.h"
#include "can_controller.h"
#include <PinConfig.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// Conditional includes based on controller types
#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#include "../motor/MotorTypes.h"
#endif

#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif

#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif

namespace CANopen_Slave
{
    // ========================================================================
    // Private State Variables
    // ========================================================================
    
    static uint8_t nodeId = 0;
    static uint32_t deviceType = CANOPEN_DEVICE_TYPE_GENERIC;
    static CANopen_NMT_State nmtState = NMT_STATE_BOOTUP;
    static uint8_t errorRegister = 0;
    static uint16_t heartbeatInterval_ms = 1000;
    static unsigned long lastHeartbeatTime = 0;
    
    // ========================================================================
    // Object Dictionary Storage
    // ========================================================================
    
    // Standard OD entries
    static uint32_t od_deviceType = 0;
    static uint16_t od_producerHeartbeat = 1000;
    static uint32_t od_vendorId = 0x00002E9C;  // UC2 vendor ID
    static uint32_t od_productCode = 0;
    static uint32_t od_revisionNumber = 1;
    static uint32_t od_serialNumber = 0;
    
    // ========================================================================
    // Helper Functions
    // ========================================================================
    
    /**
     * @brief Get COB-ID for SDO response (TX)
     */
    static inline uint16_t getSdoTxCobId()
    {
        return CANOPEN_FUNC_SDO_TX + nodeId;
    }
    
    /**
     * @brief Get COB-ID for heartbeat
     */
    static inline uint16_t getHeartbeatCobId()
    {
        return CANOPEN_FUNC_HEARTBEAT + nodeId;
    }
    
    /**
     * @brief Get COB-ID for emergency
     */
    static inline uint16_t getEmcyCobId()
    {
        return CANOPEN_FUNC_EMCY + nodeId;
    }
    
    /**
     * @brief Get COB-ID for TPDO
     */
    static inline uint16_t getTpdoCobId(uint8_t pdoNum)
    {
        uint16_t base = 0;
        switch (pdoNum) {
            case 1: base = CANOPEN_FUNC_TPDO1; break;
            case 2: base = CANOPEN_FUNC_TPDO2; break;
            case 3: base = CANOPEN_FUNC_TPDO3; break;
            case 4: base = CANOPEN_FUNC_TPDO4; break;
            default: return 0;
        }
        return base + nodeId;
    }
    
    /**
     * @brief Send CAN message with standard 11-bit identifier
     * Wrapper around can_controller::sendCanMessage that uses COB-ID directly
     */
    static int sendCanOpenMessage(uint16_t cobId, const uint8_t* data, uint8_t length)
    {
        // For CANopen, we use COB-ID as the CAN identifier
        // The existing sendCanMessage uses receiver ID which maps to COB-ID concept
        return can_controller::sendCanMessage(cobId, data, length);
    }
    
    // ========================================================================
    // Public Functions - Initialization
    // ========================================================================
    
    void init(uint8_t _nodeId, uint32_t _deviceType, uint16_t _heartbeatInterval_ms)
    {
        nodeId = _nodeId;
        deviceType = _deviceType;
        heartbeatInterval_ms = _heartbeatInterval_ms;
        nmtState = NMT_STATE_PREOPERATIONAL;  // Start in pre-operational
        errorRegister = 0;
        lastHeartbeatTime = 0;
        
        // Initialize OD entries
        od_deviceType = deviceType;
        od_producerHeartbeat = heartbeatInterval_ms;
        od_productCode = (deviceType & 0xFFFF);
        
        // Generate serial number from ESP32 MAC address
        #ifdef ARDUINO
        uint64_t chipId = ESP.getEfuseMac();
        #else
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        uint64_t chipId = 0;
        for (int i = 0; i < 6; i++) {
            chipId |= ((uint64_t)mac[i]) << (i * 8);
        }
        #endif
        od_serialNumber = (uint32_t)(chipId & 0xFFFFFFFF);
        
        log_i("CANopen Slave initialized: Node ID=%u, Device Type=0x%08X", nodeId, deviceType);
    }
    
    void setHeartbeatInterval(uint16_t interval_ms)
    {
        heartbeatInterval_ms = interval_ms;
        od_producerHeartbeat = interval_ms;
    }
    
    CANopen_NMT_State getNmtState()
    {
        return nmtState;
    }
    
    // ========================================================================
    // NMT Functions
    // ========================================================================
    
    bool processNmtCommand(const CANopen_NMT_Message* msg)
    {
        if (msg == nullptr) return false;
        
        // Check if command is for this node or broadcast (nodeId == 0)
        if (msg->nodeId != 0 && msg->nodeId != nodeId) {
            return false;  // Not for us
        }
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Received NMT command: 0x%02X for node %u", msg->command, msg->nodeId);
        
        switch (msg->command) {
            case NMT_CMD_START_REMOTE_NODE:
                setNmtState(NMT_STATE_OPERATIONAL);
                break;
                
            case NMT_CMD_STOP_REMOTE_NODE:
                setNmtState(NMT_STATE_STOPPED);
                break;
                
            case NMT_CMD_ENTER_PREOPERATIONAL:
                setNmtState(NMT_STATE_PREOPERATIONAL);
                break;
                
            case NMT_CMD_RESET_NODE:
                log_i("NMT Reset Node command received, restarting...");
                vTaskDelay(pdMS_TO_TICKS(100));  // Brief delay
                esp_restart();
                break;
                
            case NMT_CMD_RESET_COMMUNICATION:
                log_i("NMT Reset Communication command received");
                setNmtState(NMT_STATE_PREOPERATIONAL);
                // Re-initialize CAN parameters if needed
                break;
                
            default:
                log_w("Unknown NMT command: 0x%02X", msg->command);
                return false;
        }
        
        return true;
    }
    
    void setNmtState(CANopen_NMT_State newState)
    {
        if (nmtState != newState) {
            log_i("NMT state change: %u -> %u", nmtState, newState);
            nmtState = newState;
            
            // Send heartbeat immediately on state change
            sendHeartbeat();
        }
    }
    
    void sendBootup()
    {
        CANopen_Heartbeat bootup;
        bootup.state = NMT_STATE_BOOTUP;
        
        uint16_t cobId = getHeartbeatCobId();
        sendCanOpenMessage(cobId, (const uint8_t*)&bootup, sizeof(bootup));
        
        log_i("Sent CANopen boot-up message (COB-ID: 0x%03X)", cobId);
        
        // Transition to pre-operational after boot-up
        nmtState = NMT_STATE_PREOPERATIONAL;
    }
    
    // ========================================================================
    // Heartbeat Functions
    // ========================================================================
    
    void updateHeartbeat()
    {
        if (heartbeatInterval_ms == 0) {
            return;  // Heartbeat disabled
        }
        
        unsigned long currentTime = millis();
        if (currentTime - lastHeartbeatTime >= heartbeatInterval_ms) {
            sendHeartbeat();
            lastHeartbeatTime = currentTime;
        }
    }
    
    void sendHeartbeat()
    {
        CANopen_Heartbeat heartbeat;
        heartbeat.state = nmtState;
        
        uint16_t cobId = getHeartbeatCobId();
        sendCanOpenMessage(cobId, (const uint8_t*)&heartbeat, sizeof(heartbeat));
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Sent heartbeat: state=%u (COB-ID: 0x%03X)", nmtState, cobId);
    }
    
    // ========================================================================
    // Emergency Functions
    // ========================================================================
    
    void sendEmergency(uint16_t errorCode, uint8_t errReg, const uint8_t* mfrData)
    {
        CANopen_EMCY_Message emcy;
        emcy.errorCode = errorCode;
        emcy.errorRegister = errReg;
        
        // Copy manufacturer-specific data
        if (mfrData != nullptr) {
            memcpy(emcy.mfrSpecific, mfrData, 5);
        } else {
            memset(emcy.mfrSpecific, 0, 5);
        }
        
        // Update error register
        errorRegister = errReg;
        
        uint16_t cobId = getEmcyCobId();
        sendCanOpenMessage(cobId, (const uint8_t*)&emcy, sizeof(emcy));
        
        log_e("Sent EMCY: code=0x%04X, register=0x%02X (COB-ID: 0x%03X)", 
              errorCode, errReg, cobId);
    }
    
    void clearEmergency()
    {
        sendEmergency(EMCY_NO_ERROR, 0, nullptr);
        errorRegister = 0;
        log_i("Emergency cleared");
    }
    
    // ========================================================================
    // SDO Server Functions
    // ========================================================================
    
    bool processSdoRequest(const CANopen_SDO_Message* msg)
    {
        if (msg == nullptr) return false;
        
        uint8_t cs = msg->commandSpecifier;
        uint16_t index = msg->index;
        uint8_t subIndex = msg->subIndex;
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("SDO Request: CS=0x%02X, Index=0x%04X, SubIndex=%u", cs, index, subIndex);
        
        CANopen_SDO_Message response;
        response.index = index;
        response.subIndex = subIndex;
        
        // Handle read (upload) request
        if ((cs & 0xE0) == SDO_CCS_UPLOAD_INITIATE) {
            uint8_t data[4];
            uint8_t dataSize = 4;
            
            uint32_t abortCode = odRead(index, subIndex, data, &dataSize);
            
            if (abortCode != 0) {
                sendSdoAbort(index, subIndex, abortCode);
                return false;
            }
            
            // Send expedited response
            response.commandSpecifier = SDO_SCS_UPLOAD_RESPONSE | SDO_EXPEDITED_FLAG | SDO_SIZE_INDICATED;
            
            // Set size bits based on dataSize
            switch (dataSize) {
                case 1: response.commandSpecifier |= SDO_SIZE_1_BYTE; break;
                case 2: response.commandSpecifier |= SDO_SIZE_2_BYTES; break;
                case 3: response.commandSpecifier |= SDO_SIZE_3_BYTES; break;
                default: response.commandSpecifier |= SDO_SIZE_4_BYTES; break;
            }
            
            memcpy(response.data, data, dataSize);
            if (dataSize < 4) {
                memset(response.data + dataSize, 0, 4 - dataSize);
            }
            
            sendSdoResponse(&response);
            return true;
        }
        // Handle write (download) request
        else if ((cs & 0xE0) == SDO_CCS_DOWNLOAD_INITIATE) {
            uint8_t dataSize = 4;
            
            // Determine data size from command specifier
            if (cs & SDO_SIZE_INDICATED) {
                uint8_t n = (cs >> 2) & 0x03;
                dataSize = 4 - n;
            }
            
            uint32_t abortCode = odWrite(index, subIndex, msg->data, dataSize);
            
            if (abortCode != 0) {
                sendSdoAbort(index, subIndex, abortCode);
                return false;
            }
            
            // Send confirmation
            response.commandSpecifier = SDO_SCS_DOWNLOAD_RESPONSE;
            memset(response.data, 0, 4);
            
            sendSdoResponse(&response);
            return true;
        }
        
        // Unknown command
        sendSdoAbort(index, subIndex, SDO_ABORT_INVALID_CS);
        return false;
    }
    
    void sendSdoResponse(const CANopen_SDO_Message* msg)
    {
        uint16_t cobId = getSdoTxCobId();
        sendCanOpenMessage(cobId, (const uint8_t*)msg, sizeof(CANopen_SDO_Message));
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Sent SDO response: CS=0x%02X, Index=0x%04X (COB-ID: 0x%03X)", 
                  msg->commandSpecifier, msg->index, cobId);
    }
    
    void sendSdoAbort(uint16_t index, uint8_t subIndex, uint32_t abortCode)
    {
        CANopen_SDO_Message abort;
        abort.commandSpecifier = SDO_SCS_ABORT;
        abort.index = index;
        abort.subIndex = subIndex;
        abort.abortCode = abortCode;
        
        sendSdoResponse(&abort);
        
        log_w("SDO Abort: Index=0x%04X, SubIndex=%u, Code=0x%08X", 
              index, subIndex, abortCode);
    }
    
    // ========================================================================
    // PDO Functions
    // ========================================================================
    
    bool arePdosEnabled()
    {
        return (nmtState == NMT_STATE_OPERATIONAL);
    }
    
    bool processRpdo(uint16_t cobId, const uint8_t* data, uint8_t length)
    {
        if (!arePdosEnabled()) {
            log_w("Received RPDO in non-operational state, ignoring");
            return false;
        }
        
        // Determine which RPDO based on COB-ID
        if (cobId == (CANOPEN_FUNC_RPDO1 + nodeId)) {
            // RPDO1 - Device-specific handling
            #ifdef MOTOR_CONTROLLER
            if (length >= sizeof(CANopen_Motor_RPDO1)) {
                CANopen_Motor_RPDO1* cmd = (CANopen_Motor_RPDO1*)data;
                processMotorCommand(cmd->targetPosition, cmd->speed);
                return true;
            }
            #endif
            
            #ifdef LASER_CONTROLLER
            if (length >= sizeof(CANopen_Laser_RPDO1)) {
                CANopen_Laser_RPDO1* cmd = (CANopen_Laser_RPDO1*)data;
                processLaserCommand(cmd->intensity, cmd->laserId, cmd->enable);
                return true;
            }
            #endif
            
            #ifdef LED_CONTROLLER
            if (length >= sizeof(CANopen_LED_RPDO1)) {
                CANopen_LED_RPDO1* cmd = (CANopen_LED_RPDO1*)data;
                processLedCommand(cmd->mode, cmd->red, cmd->green, cmd->blue, cmd->brightness);
                return true;
            }
            #endif
        }
        
        return false;
    }
    
    void sendTpdo(uint8_t pdoNum, const uint8_t* data, uint8_t length)
    {
        if (!arePdosEnabled()) {
            return;  // PDOs disabled in current state
        }
        
        uint16_t cobId = getTpdoCobId(pdoNum);
        if (cobId == 0) {
            log_w("Invalid PDO number: %u", pdoNum);
            return;
        }
        
        sendCanOpenMessage(cobId, data, length);
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Sent TPDO%u (COB-ID: 0x%03X, length: %u)", pdoNum, cobId, length);
    }
    
    // ========================================================================
    // Object Dictionary Access
    // ========================================================================
    
    uint32_t odRead(uint16_t index, uint8_t subIndex, uint8_t* data, uint8_t* dataSize)
    {
        // Standard communication objects
        if (index == OD_DEVICE_TYPE && subIndex == 0) {
            memcpy(data, &od_deviceType, 4);
            *dataSize = 4;
            return 0;
        }
        
        if (index == OD_ERROR_REGISTER && subIndex == 0) {
            data[0] = errorRegister;
            *dataSize = 1;
            return 0;
        }
        
        if (index == OD_PRODUCER_HEARTBEAT_TIME && subIndex == 0) {
            memcpy(data, &od_producerHeartbeat, 2);
            *dataSize = 2;
            return 0;
        }
        
        if (index == OD_IDENTITY_OBJECT) {
            switch (subIndex) {
                case 1:  // Vendor ID
                    memcpy(data, &od_vendorId, 4);
                    *dataSize = 4;
                    return 0;
                case 2:  // Product code
                    memcpy(data, &od_productCode, 4);
                    *dataSize = 4;
                    return 0;
                case 3:  // Revision number
                    memcpy(data, &od_revisionNumber, 4);
                    *dataSize = 4;
                    return 0;
                case 4:  // Serial number
                    memcpy(data, &od_serialNumber, 4);
                    *dataSize = 4;
                    return 0;
            }
        }
        
        // Device-specific objects would go here
        // For now, return "object does not exist"
        return SDO_ABORT_OBJECT_NOT_EXIST;
    }
    
    uint32_t odWrite(uint16_t index, uint8_t subIndex, const uint8_t* data, uint8_t dataSize)
    {
        // Heartbeat interval is writable
        if (index == OD_PRODUCER_HEARTBEAT_TIME && subIndex == 0 && dataSize == 2) {
            memcpy(&od_producerHeartbeat, data, 2);
            heartbeatInterval_ms = od_producerHeartbeat;
            log_i("Heartbeat interval set to %u ms", heartbeatInterval_ms);
            return 0;
        }
        
        // Most standard objects are read-only
        if (index >= 0x1000 && index < 0x2000) {
            return SDO_ABORT_READ_ONLY;
        }
        
        // Device-specific writable objects would go here
        
        return SDO_ABORT_OBJECT_NOT_EXIST;
    }
    
    // ========================================================================
    // Main Loop
    // ========================================================================
    
    void loop()
    {
        updateHeartbeat();
        
        // Additional periodic tasks could go here
    }
    
    // ========================================================================
    // Device-Specific Helpers
    // ========================================================================
    
    #ifdef MOTOR_CONTROLLER
    void sendMotorStatus(int32_t position, uint8_t statusWord)
    {
        CANopen_Motor_TPDO1 status;
        status.currentPosition = position;
        status.statusWord = statusWord;
        memset(status.reserved, 0, sizeof(status.reserved));
        
        sendTpdo(1, (const uint8_t*)&status, sizeof(status));
    }
    
    void processMotorCommand(int32_t targetPos, uint32_t speed)
    {
        // Forward to existing motor controller
        using namespace FocusMotor;
        
        // Validate motor axis ID is within bounds
        int axisId = pinConfig.REMOTE_MOTOR_AXIS_ID;
        if (axisId < 0 || axisId >= MOTOR_AXIS_COUNT) {
            log_e("Invalid motor axis ID: %d (max: %d)", axisId, MOTOR_AXIS_COUNT - 1);
            return;
        }
        
        Stepper mStepper = static_cast<Stepper>(axisId);
        
        getData()[mStepper]->targetPosition = targetPos;
        getData()[mStepper]->speed = speed;
        getData()[mStepper]->isStop = false;
        
        toggleStepper(mStepper, false, 0);
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Motor command: pos=%ld, speed=%lu", targetPos, speed);
    }
    #endif
    
    #ifdef LASER_CONTROLLER
    void processLaserCommand(uint16_t intensity, uint8_t laserId, uint8_t enable)
    {
        // Forward to existing laser controller
        LaserController::setLaserVal(laserId, enable ? intensity : 0);
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Laser command: id=%u, intensity=%u, enable=%u", laserId, intensity, enable);
    }
    #endif
    
    #ifdef LED_CONTROLLER
    void processLedCommand(uint8_t mode, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
    {
        // Forward to existing LED controller
        using namespace LedController;
        LedCommand cmd;
        cmd.mode = (LedMode) mode;
        cmd.r = r;
        cmd.g = g;
        cmd.b = b;
        // Additional mapping would go here
        
        execLedCommand(cmd);
        
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("LED command: mode=%u, RGB=(%u,%u,%u), brightness=%u", 
                  mode, r, g, b, brightness);
    }
    #endif
    
} // namespace CANopen_Slave
