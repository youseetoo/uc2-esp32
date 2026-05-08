/**
 * @file CanOpenOTAStreaming.cpp
 * @brief Master-side OTA streaming to a slave via CANopen SDO domain transfer
 *
 * Uses CANopenModule::writeSDO*() and writeSDODomain() to push firmware
 * to the slave. CANopenNode handles segmented/block SDO automatically.
 */

#include "CanOpenOTAStreaming.h"

#ifdef CAN_CONTROLLER_CANOPEN

#include "CANopenModule.h"
#include "UC2_OD_Indices.h"
#include <esp_log.h>
#include <rom/crc.h>

extern "C" {
#include "OD.h"
}

#define TAG "OTA_MASTER"

// ============================================================================
// Public API
// ============================================================================

bool CanOpenOTAStreaming::flashSlave(uint8_t nodeId, const uint8_t* data,
                                     size_t dataSize, uint32_t crc32) {
    ESP_LOGI(TAG, "Flashing node %d: %u bytes, CRC32=0x%08lx",
             nodeId, (unsigned)dataSize, (unsigned long)crc32);

    // Step 1: Write CRC32 to slave (so it can verify after transfer)
    if (!CANopenModule::writeSDO_u32(nodeId, UC2_OD::OTA_FIRMWARE_CRC32, 0, crc32)) {
        ESP_LOGE(TAG, "Failed to write CRC32 to node %d", nodeId);
        return false;
    }

    // Step 2: Write firmware size (triggers OTA begin on slave)
    if (!CANopenModule::writeSDO_u32(nodeId, UC2_OD::OTA_FIRMWARE_SIZE, 0, (uint32_t)dataSize)) {
        ESP_LOGE(TAG, "Failed to write firmware size to node %d", nodeId);
        return false;
    }

    // Brief delay for slave to initialize OTA partition
    vTaskDelay(pdMS_TO_TICKS(200));

    // Step 3: Write firmware data via SDO domain transfer
    // CANopenNode handles segmented/block transfer automatically
    if (!CANopenModule::writeSDODomain(nodeId, UC2_OD::OTA_FIRMWARE_DATA, 0,
                                        data, dataSize)) {
        ESP_LOGE(TAG, "SDO domain write failed for node %d", nodeId);
        return false;
    }

    ESP_LOGI(TAG, "OTA transfer complete for node %d, waiting for verification...", nodeId);

    // Step 4: Read back status (slave should be COMPLETE or rebooting)
    vTaskDelay(pdMS_TO_TICKS(500));
    uint8_t status = readSlaveStatus(nodeId);
    // Node may have already rebooted, so timeout is acceptable
    if (status == 0x03) {  // CANOPEN_OTA_COMPLETE
        ESP_LOGI(TAG, "Node %d: OTA verified and committed", nodeId);
    } else if (status == 0xFF) {
        ESP_LOGI(TAG, "Node %d: status read failed (likely rebooting — good)", nodeId);
    } else {
        ESP_LOGW(TAG, "Node %d: unexpected status 0x%02x", nodeId, status);
    }

    return true;
}

uint8_t CanOpenOTAStreaming::readSlaveStatus(uint8_t nodeId) {
    uint8_t status = 0xFF;
    size_t readSize = 0;
    if (CANopenModule::readSDO(nodeId, UC2_OD::OTA_STATUS, 0,
                                &status, sizeof(status), &readSize)) {
        return status;
    }
    return 0xFF;
}

int CanOpenOTAStreaming::actFromJson(cJSON* doc) {
    cJSON* otaObj = cJSON_GetObjectItem(doc, "ota");
    if (!otaObj) {
        ESP_LOGE(TAG, "Missing 'ota' object");
        return -1;
    }

    cJSON* nodeIdItem = cJSON_GetObjectItem(otaObj, "nodeId");
    cJSON* sizeItem   = cJSON_GetObjectItem(otaObj, "size");
    cJSON* crc32Item  = cJSON_GetObjectItem(otaObj, "crc32");

    if (!nodeIdItem || !sizeItem) {
        ESP_LOGE(TAG, "Missing 'nodeId' or 'size' in ota object");
        return -1;
    }

    uint8_t  nodeId = (uint8_t)nodeIdItem->valueint;
    uint32_t size   = (uint32_t)sizeItem->valuedouble;

    uint32_t crc32 = 0;
    if (crc32Item && cJSON_IsString(crc32Item)) {
        crc32 = (uint32_t)strtoul(crc32Item->valuestring, NULL, 16);
    } else if (crc32Item && cJSON_IsNumber(crc32Item)) {
        crc32 = (uint32_t)crc32Item->valuedouble;
    }

    ESP_LOGI(TAG, "OTA command: node=%d, size=%lu, crc32=0x%08lx",
             nodeId, (unsigned long)size, (unsigned long)crc32);

    // For now, log that we need data from serial.
    // The actual binary receive + flash is handled by the serial binary mode
    // which calls flashSlave() once the data is buffered.
    // TODO: Implement serial binary receive mode for large firmware images
    // For small test images or SD card, caller can use flashSlave() directly.
    ESP_LOGI(TAG, "OTA start acknowledged — awaiting firmware data");

    return 0;
}

#endif // CAN_CONTROLLER_CANOPEN
