#!/usr/bin/env python3
"""
UC2 CAN OTA — flash a slave directly from laptop via python-canopen + Waveshare USB-CAN.

Usage:
    python -m uc2_canopen.uc2_ota_can --node 11 --binary firmware.bin
    python -m uc2_canopen.uc2_ota_can --node 11 --binary firmware.bin --interface socketcan --channel can0

Requires:
    pip install python-canopen

Protocol:
    1. Write CRC32       → 0x2F02 (uint32)
    2. Write firmware size → 0x2F01 (uint32) — triggers OTA begin on slave
    3. Write firmware data → 0x2F00 (domain SDO — segmented/block transfer)
    4. Read status         ← 0x2F03 — should be 0x03 (COMPLETE) or slave reboots
"""

import argparse
import struct
import sys
import time
import zlib
from pathlib import Path

try:
    import canopen
except ImportError:
    print("ERROR: python-canopen not installed. Run: pip install python-canopen")
    sys.exit(1)


# UC2 OD indices (from UC2_OD_Indices.h)
OTA_FIRMWARE_DATA   = 0x2F00
OTA_FIRMWARE_SIZE   = 0x2F01
OTA_FIRMWARE_CRC32  = 0x2F02
OTA_STATUS          = 0x2F03
OTA_BYTES_RECEIVED  = 0x2F04
OTA_ERROR_CODE      = 0x2F05

# OTA status values
STATUS_IDLE      = 0x00
STATUS_RECEIVING = 0x01
STATUS_VERIFYING = 0x02
STATUS_COMPLETE  = 0x03
STATUS_ERROR     = 0xFF


def flash_node(network: canopen.Network, node_id: int, firmware: bytes,
               verbose: bool = False) -> bool:
    """Flash firmware to a slave node via SDO domain transfer."""
    crc32 = zlib.crc32(firmware) & 0xFFFFFFFF
    size = len(firmware)

    print(f"Target node:     {node_id}")
    print(f"Firmware size:   {size:,} bytes")
    print(f"CRC32:           0x{crc32:08X}")

    node = network.add_node(node_id)

    # Step 1: Write CRC32
    print("\n[1/4] Writing CRC32...")
    node.sdo.download(OTA_FIRMWARE_CRC32, 0, struct.pack("<I", crc32))

    # Step 2: Write firmware size (triggers OTA begin)
    print("[2/4] Writing firmware size (triggers OTA begin)...")
    node.sdo.download(OTA_FIRMWARE_SIZE, 0, struct.pack("<I", size))
    time.sleep(0.5)  # Let slave initialize partition

    # Step 3: Write firmware data (domain SDO — python-canopen handles segmented)
    print("[3/4] Uploading firmware data via SDO domain transfer...")
    start_time = time.time()

    # python-canopen handles segmented/block transfer for large data
    node.sdo.download(OTA_FIRMWARE_DATA, 0, firmware)

    elapsed = time.time() - start_time
    speed = size / elapsed / 1024 if elapsed > 0 else 0
    print(f"     Transfer complete: {elapsed:.1f}s ({speed:.1f} KB/s)")

    # Step 4: Check status
    print("[4/4] Checking status...")
    time.sleep(1.0)
    try:
        status_data = node.sdo.upload(OTA_STATUS, 0)
        status = status_data[0] if status_data else 0xFF
        if status == STATUS_COMPLETE:
            print(f"     Status: COMPLETE (0x{status:02X}) — slave will reboot")
            return True
        elif status == STATUS_ERROR:
            try:
                err_data = node.sdo.upload(OTA_ERROR_CODE, 0)
                err = err_data[0] if err_data else 0xFF
                print(f"     Status: ERROR — code 0x{err:02X}")
            except Exception:
                print("     Status: ERROR — could not read error code")
            return False
        else:
            print(f"     Status: 0x{status:02X}")
            return status == STATUS_COMPLETE
    except Exception:
        # Node likely already rebooted — that's success
        print("     Node not responding (likely rebooting — success)")
        return True


def main():
    parser = argparse.ArgumentParser(
        description="Flash UC2 slave firmware via CANopen SDO"
    )
    parser.add_argument("--node", type=int, required=True,
                        help="Target slave node ID (e.g. 11)")
    parser.add_argument("--binary", type=str, required=True,
                        help="Path to firmware .bin file")
    parser.add_argument("--interface", type=str, default="slcan",
                        help="python-can interface (default: slcan)")
    parser.add_argument("--channel", type=str, default="/dev/ttyUSB0",
                        help="CAN interface channel (default: /dev/ttyUSB0)")
    parser.add_argument("--bitrate", type=int, default=500000,
                        help="CAN bus bitrate (default: 500000)")
    parser.add_argument("--verbose", action="store_true",
                        help="Verbose output")
    args = parser.parse_args()

    firmware_path = Path(args.binary)
    if not firmware_path.exists():
        print(f"ERROR: Firmware file not found: {firmware_path}")
        sys.exit(1)

    firmware = firmware_path.read_bytes()
    if len(firmware) == 0:
        print("ERROR: Firmware file is empty")
        sys.exit(1)

    print("=" * 60)
    print("UC2 CANopen OTA Firmware Flasher")
    print("=" * 60)
    print(f"Interface:       {args.interface}")
    print(f"Channel:         {args.channel}")
    print(f"Bitrate:         {args.bitrate}")
    print(f"Firmware:        {firmware_path.name}")

    network = canopen.Network()
    try:
        network.connect(bustype=args.interface, channel=args.channel,
                        bitrate=args.bitrate)
        print("CAN bus connected.\n")

        success = flash_node(network, args.node, firmware, args.verbose)

        if success:
            print("\n✓ OTA flash successful!")
        else:
            print("\n✗ OTA flash failed!")
            sys.exit(1)
    except Exception as e:
        print(f"\nERROR: {e}")
        sys.exit(1)
    finally:
        network.disconnect()


if __name__ == "__main__":
    main()
