#pragma once
#include <stdint.h>

// Maximum SSID and password lengths for WiFi credentials
#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64

/**
 * @brief Structure to hold OTA WiFi credentials sent over CAN bus
 * 
 * This structure is sent from the master to a slave device via CAN bus
 * to initiate an OTA update process. The slave will connect to the 
 * specified WiFi network and start an OTA server.
 */
typedef struct {
    char ssid[MAX_SSID_LENGTH];         // WiFi SSID
    char password[MAX_PASSWORD_LENGTH]; // WiFi password
    uint32_t timeout_ms;                 // OTA server timeout in milliseconds (0 = infinite)
} OtaWifiCredentials;

/**
 * @brief OTA acknowledgment structure sent back to master
 */
typedef struct {
    uint8_t status;  // 0 = success, 1 = wifi connection failed, 2 = OTA start failed
    uint8_t canId;   // CAN ID of the responding device
    uint8_t ipAddress[4]; // IP address of the device (e.g., 192.168.2.137 -> {192, 168, 2, 137})
} OtaAck;

/**
 * @brief CAN device scan response structure
 * 
 * This structure is sent back from a slave device when it receives a SCAN_REQUEST.
 * It provides basic device information for network discovery.
 */
typedef struct {
    uint8_t canId;        // CAN ID of the responding device
    uint8_t deviceType;   // Device type: 0=motor, 1=laser, 2=LED, 3=galvo, 4=master, 0xFF=unknown
    uint8_t status;       // Device status: 0=idle, 1=busy, 0xFF=error
} ScanResponse;

/**
 * @brief Soft limit configuration structure
 * 
 * This structure is sent from master to slave to configure motor soft limits.
 * Soft limits prevent the motor from moving beyond specified boundaries.
 */
typedef struct {
    uint8_t axis;              // Motor axis (0=A, 1=X, 2=Y, 3=Z, etc.)
    int32_t minPos;            // Minimum allowed position in steps
    int32_t maxPos;            // Maximum allowed position in steps
    uint8_t enabled;           // 1=enabled, 0=disabled
} SoftLimitData;
