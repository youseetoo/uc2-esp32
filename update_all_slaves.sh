#!/bin/bash
# Example script to update multiple CAN slave devices via OTA
# Modify the variables below to match your setup

SERIAL_PORT="/dev/ttyUSB0"
WIFI_SSID="YourWiFiNetwork"
WIFI_PASSWORD="YourWiFiPassword"
OTA_TIMEOUT=300000  # 5 minutes in milliseconds

# CAN IDs for your devices
MOTOR_X_ID=11
MOTOR_Y_ID=12
MOTOR_Z_ID=13
LED_ID=30

echo "=========================================="
echo "OTA Update Script for UC2 CAN Network"
echo "=========================================="
echo ""

# Function to send OTA command and wait
send_ota_and_wait() {
    local can_id=$1
    local device_name=$2
    local env_name=$3
    
    echo "Initiating OTA for ${device_name} (CAN ID: ${can_id})..."
    
    python3 PYTHON/ota_update_via_can.py \
        --port "${SERIAL_PORT}" \
        --canid ${can_id} \
        --ssid "${WIFI_SSID}" \
        --password "${WIFI_PASSWORD}" \
        --timeout ${OTA_TIMEOUT}
    
    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to send OTA command to ${device_name}"
        return 1
    fi
    
    echo "Waiting 40 seconds for device to connect to WiFi..."
    sleep 40
    
    # Calculate hostname
    printf -v hex_id "%X" ${can_id}
    hostname="UC2-CAN-${hex_id}.local"
    
    echo "Uploading firmware to ${hostname}..."
    
    platformio run -e "${env_name}" -t upload --upload-port "${hostname}"
    
    if [ $? -eq 0 ]; then
        echo "✓ Successfully updated ${device_name}"
        return 0
    else
        echo "✗ Failed to upload firmware to ${device_name}"
        return 1
    fi
}

# Main update sequence
echo "Starting OTA update sequence..."
echo ""

# Update Motor X
send_ota_and_wait ${MOTOR_X_ID} "Motor X" "seeed_xiao_esp32s3_can_slave_motor"
echo ""
sleep 5

# Update Motor Y
send_ota_and_wait ${MOTOR_Y_ID} "Motor Y" "seeed_xiao_esp32s3_can_slave_motor"
echo ""
sleep 5

# Update Motor Z
send_ota_and_wait ${MOTOR_Z_ID} "Motor Z" "seeed_xiao_esp32s3_can_slave_motor"
echo ""
sleep 5

# Update LED Controller
send_ota_and_wait ${LED_ID} "LED Controller" "seeed_xiao_esp32s3_can_slave_led_debug"
echo ""

echo "=========================================="
echo "OTA Update Sequence Complete!"
echo "=========================================="
