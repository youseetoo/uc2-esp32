#!/usr/bin/env python3
"""
UC2 CAN OTA — flash a slave directly from laptop via python-canopen + Waveshare USB-CAN.

Usage:
    python -m uc2_canopen.uc2_ota_can --node 11 --binary firmware.bin
    python -m uc2_canopen.uc2_ota_can --node 11 --binary firmware.bin --interface socketcan --channel can0
    python tools/canopen/uc2_ota_can.py --node 11 --binary firmware.bin --waveshare
    python tools/canopen/uc2_ota_can.py --node 11 --binary /path/to/firmware.bin --waveshare --channel /dev/cu.wchusbserial110

    /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP/.venv/bin/python tools/canopen/uc2_ota_can.py --node 11 --binary /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST/binaries/latest/esp32_seeed_xiao_esp32s3_can_slave_motor.bin --waveshare --channel /dev/cu.wchusbserial110

    


Requires:
    pip install python-canopen

Protocol:
    1. Write CRC32       → 0x2F02 (uint32)
    2. Write firmware size → 0x2F01 (uint32) — triggers OTA begin on slave
    3. Write firmware data → 0x2F00 (domain SDO — segmented/block transfer)
    4. Read status         ← 0x2F03 — should be 0x03 (COMPLETE) or slave reboots
"""

import argparse
import glob
import os
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


def _import_waveshare_bus():
    """Locate and import the WaveshareBus class.

    Tries the installed `uc2canopen` package first, then falls back to the
    sibling UC2-REST-CANOPEN repository checkout that lives next to uc2-ESP.
    """
    try:
        from waveshare_bus import WaveshareBus  # type: ignore
        return WaveshareBus
    except ImportError:
        print("WaveshareBus not found in installed packages, trying sibling UC2-REST-CANOPEN repo...")
        pass

    here = Path(__file__).resolve()
    candidates = [
        here.parents[2] / "UC2-REST-CANOPEN" / "src",            # workspace sibling
        here.parents[3] / "UC2-REST-CANOPEN" / "src",            # one level up
        Path.home() / "Dropbox" / "Dokumente" / "Promotion" / "PROJECTS"
            / "UC2-REST-CANOPEN" / "src",                        # known dev path
    ]
    for c in candidates:
        if (c / "uc2canopen" / "waveshare_bus.py").exists():
            sys.path.insert(0, str(c))
            try:
                from uc2canopen.waveshare_bus import WaveshareBus  # type: ignore
                return WaveshareBus
            except ImportError:
                continue
    raise ImportError(
        "WaveshareBus not found. Install UC2-REST-CANOPEN "
        "(pip install -e <path>/UC2-REST-CANOPEN) or place it next to uc2-ESP."
    )


def _autodetect_waveshare_port() -> str:
    """Best-effort autodetection of the Waveshare USB-CAN-A serial port."""
    patterns = [
        "/dev/cu.wchusbserial*",
        "/dev/cu.usbserial*",
        "/dev/tty.wchusbserial*",
        "/dev/tty.usbserial*",
        "/dev/ttyUSB*",
    ]
    for p in patterns:
        matches = sorted(glob.glob(p))
        if matches:
            return matches[0]
    raise RuntimeError(
        "Could not auto-detect a Waveshare USB-CAN-A port. "
        "Pass --channel /dev/ttyXXX explicitly."
    )


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
                        help="python-can interface (default: slcan). Ignored when --waveshare is set.")
    parser.add_argument("--channel", type=str, default="/dev/ttyUSB0",
                        help="CAN interface channel (default: /dev/ttyUSB0)")
    parser.add_argument("--bitrate", type=int, default=500000,
                        help="CAN bus bitrate (default: 500000)")
    parser.add_argument("--waveshare", action="store_true",
                        help="Use Waveshare USB-CAN-A adapter (auto-detect port if --channel not set)")
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

    # Auto-detect Waveshare port when requested but channel left at default
    waveshare_bus = None
    if args.waveshare:
        WaveshareBus = _import_waveshare_bus()
        channel = args.channel
        if channel == "/dev/ttyUSB0":  # default value, override with autodetect
            try:
                channel = _autodetect_waveshare_port()
            except RuntimeError as e:
                print(f"ERROR: {e}")
                sys.exit(1)
        print(f"Interface:       waveshare USB-CAN-A")
        print(f"Channel:         {channel}")
        print(f"Bitrate:         {args.bitrate}")
        print(f"Firmware:        {firmware_path.name}")
        waveshare_bus = WaveshareBus(channel=channel, bitrate=args.bitrate, serial_baudrate=2000000)
    else:
        print(f"Interface:       {args.interface}")
        print(f"Channel:         {args.channel}")
        print(f"Bitrate:         {args.bitrate}")
        print(f"Firmware:        {firmware_path.name}")

    network = canopen.Network()
    # The Waveshare USB-CAN-A serial path adds latency; bump the default
    # SDO response timeout (0.3 s) to avoid spurious 0x05040000 aborts.
    try:
        canopen.sdo.client.RESPONSE_TIMEOUT = 5.0  # type: ignore[attr-defined]
    except Exception:
        pass
    try:
        if waveshare_bus is not None:
            # Newer python-canopen ignores `bus=` in connect(); assign directly
            # so connect() skips can.Bus creation and only starts the notifier.
            network.bus = waveshare_bus
            network.connect()
        else:
            network.connect(interface=args.interface, channel=args.channel,
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
