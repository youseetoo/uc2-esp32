#!/bin/bash

# Exit immediately if any command returns a non-zero (error) status
set -e

# Define paths for readability
PIO="/Users/bene/.platformio/penv/bin/platformio"
PIO_PYTHON="/Users/bene/.platformio/penv/bin/python3"
ESPTOOL="/Users/bene/.platformio/packages/tool-esptoolpy/esptool.py"

echo "🚀 Starting deployment sequence..."
echo "----------------------------------------"

# Step 1: Upload to the CANopen Master
echo "📥 Step 1: Uploading to Master debug environment..."
$PIO run --target upload --environment UC2_canopen_master_debug --upload-port /dev/cu.SLAB_USBtoUART
echo "✅ Master upload complete."
echo "----------------------------------------"

# Step 2: Full Chip Erase on the Slave (Matches the Web Tool)
echo "🧹 Step 2: Performing low-level factory flash erase on Slave..."
$PIO_PYTHON $ESPTOOL --chip esp32s3 --port /dev/cu.usbmodem101 --baud 921600 erase_flash
echo "✅ Slave flash entirely erased."
echo "----------------------------------------"

# Step 3: Upload to the CANopen Slave
echo "📥 Step 3: Uploading firmware to Slave motor environment..."
$PIO run --target upload --environment UC2_canopen_slave_motor --upload-port /dev/cu.usbmodem101
echo "✅ Slave upload complete."
echo "----------------------------------------"

# Step 4: Perform CANopen OTA Serial update
echo "🛰️ Step 4: Running CANopen OTA Serial script..."
python3 /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP/tools/ota/canopen_ota_serial.py \
    --port /dev/cu.SLAB_USBtoUART \
    --node 11 \
    --binary /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST/binaries/latest/esp32_seeed_xiao_esp32s3_can_slave_motor.bin \
    --baud 921600

echo "----------------------------------------"
echo "🎉 All steps completed successfully!"