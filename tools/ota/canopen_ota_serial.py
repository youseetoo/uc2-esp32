#!/usr/bin/env python3
"""
Flash a UC2 slave's firmware via the CANopen path:

    Laptop (this script) -> serial -> ESP32 master -> CAN -> slave

The master must be running a CAN_CONTROLLER_CANOPEN build that exposes the
``/ota_start`` JSON command (see ``DeviceRouter::handleOtaStart`` and
``OtaBinaryReceive``). Once the master ACKs the preamble with
``{"ota_status":"ready", ...}`` we stream the raw firmware bytes straight
into the master's PSRAM buffer; the master verifies CRC32, then performs a
single SDO domain download to the target slave.

Usage:
    python tools/ota/canopen_ota_serial.py \\
        --port /dev/cu.usbmodem7 \\
        --node 11 \\
        --binary /path/to/firmware.bin

    python tools/ota/canopen_ota_serial.py --port /dev/cu.SLAB_USBtoUART --node 11 --binary /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST/binaries/latest/esp32_seeed_xiao_esp32s3_can_slave_motor.bin --baud 921600

{"task": "/ota_start", "ota": {"nodeId": 11, "size": 798432, "crc32": "0xD2A5FB80"}}

"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
import zlib
from pathlib import Path

try:
    import serial  # type: ignore
except ImportError:
    print("ERROR: pyserial is not installed. Run: pip install pyserial")
    sys.exit(1)


READY_TIMEOUT_S = 10.0
FLASH_TIMEOUT_S = 600.0   # SDO domain transfer can take minutes
CHUNK_SIZE      = 4096
INTER_CHUNK_DELAY_S = 0.0  # set >0 if the host overruns the master's UART


# Match standalone JSON objects on a single line. The master's framed output
# uses ``++\n<json>\n--\n`` envelopes, but bare JSON lines (e.g. emitted from
# OtaBinaryReceive::emitJson) are also handled.
_JSON_LINE_RE = re.compile(rb"\{[^\n]*\}")


def _try_extract_json_status(buf: bytes):
    """Return the first parsed JSON object on ``buf`` that contains ``ota_status``
    or ``ota_rx``, else None."""
    for match in _JSON_LINE_RE.findall(buf):
        try:
            obj = json.loads(match.decode(errors="ignore"))
        except json.JSONDecodeError:
            continue
        if isinstance(obj, dict) and ("ota_status" in obj or "ota_rx" in obj):
            return obj
    return None


def _wait_for_status(ser: serial.Serial, deadline: float, key_values=None):
    """Read until ``key_values`` matches ``ota_status`` or deadline elapses.

    ``key_values`` is an iterable of acceptable status strings (e.g.
    ``("ready",)``). Returns the matching JSON object or None on timeout.
    """
    buf = bytearray()
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf.extend(chunk)
            obj = _try_extract_json_status(bytes(buf))
            if obj is not None:
                if key_values is None:
                    return obj
                status = obj.get("ota_status")
                if status in key_values:
                    return obj
                # Any error from the master ends the session early.
                if status == "error":
                    return obj
                # Drop processed JSON to avoid re-matching it forever.
                idx = bytes(buf).rfind(b"}")
                if idx > 0:
                    del buf[: idx + 1]
        else:
            time.sleep(0.005)
    return None


def flash_via_master(port: str, baud: int, node_id: int, firmware: bytes) -> bool:
    crc32 = zlib.crc32(firmware) & 0xFFFFFFFF
    size = len(firmware)

    print(f"Target node : {node_id}")
    print(f"Firmware    : {size:,} bytes")
    print(f"CRC32       : 0x{crc32:08X}")
    print(f"Serial port : {port} @ {baud} baud")

    ser = serial.Serial(port, baud, timeout=0.05)
    try:
        time.sleep(0.5)  # let the ESP32 settle / drain boot chatter
        ser.reset_input_buffer()

        # Step 1: send the JSON preamble.
        cmd = json.dumps({
            "task": "/ota_start",
            "ota": {
                "nodeId": node_id,
                "size": size,
                "crc32": f"0x{crc32:08X}",
            },
        })
        print("\n[1/3] Sending /ota_start...")
        print(f"      {cmd}")
        ser.write((cmd + "\n").encode())
        ser.flush()

        ready = _wait_for_status(ser, time.time() + READY_TIMEOUT_S,
                                 key_values=("ready",))
        if not ready or ready.get("ota_status") != "ready":
            print(f"ERROR: master did not become ready: {ready}")
            return False
        print(f"      master ready: {ready}")

        # Step 2: stream raw firmware bytes.
        print(f"[2/3] Uploading {size:,} bytes...")
        start = time.time()
        sent = 0
        while sent < size:
            end = min(sent + CHUNK_SIZE, size)
            n = ser.write(firmware[sent:end])
            ser.flush()
            sent += n

            # Drain progress ACKs as they arrive.
            if ser.in_waiting:
                pending = ser.read(ser.in_waiting)
                for match in _JSON_LINE_RE.findall(pending):
                    try:
                        obj = json.loads(match.decode(errors="ignore"))
                    except json.JSONDecodeError:
                        continue
                    if "ota_rx" in obj:
                        rx = int(obj["ota_rx"])
                        pct = rx / size * 100.0
                        print(f"      progress: {rx:,} / {size:,} ({pct:5.1f}%)",
                              end="\r", flush=True)
                    elif obj.get("ota_status") == "error":
                        print(f"\nERROR from master: {obj}")
                        return False

            if INTER_CHUNK_DELAY_S > 0:
                time.sleep(INTER_CHUNK_DELAY_S)

        elapsed = time.time() - start
        if elapsed > 0:
            print(f"\n      transfer: {elapsed:.1f}s "
                  f"({size / elapsed / 1024:.1f} KB/s)")

        # Step 3: wait for the flashing -> success/error transition.
        print("[3/3] Waiting for slave flash result (may take minutes)...")
        result = _wait_for_status(ser, time.time() + FLASH_TIMEOUT_S,
                                  key_values=("success", "error"))
        if result is None:
            print("ERROR: timed out waiting for OTA result")
            return False
        if result.get("ota_status") == "success":
            print("\nOTA flash successful — slave will reboot.")
            return True
        print(f"\nOTA flash failed: {result}")
        return False
    finally:
        try:
            ser.close()
        except Exception:
            pass


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Flash a UC2 slave via serial -> ESP32 master -> CAN")
    parser.add_argument("--port", required=True,
                        help="Serial port of the ESP32 master "
                             "(e.g. /dev/cu.usbmodem7)")
    parser.add_argument("--baud", type=int, default=115200,
                        help="Serial baudrate (default: 115200)")
    parser.add_argument("--node", type=int, required=True,
                        help="Target slave CANopen node ID (e.g. 11)")
    parser.add_argument("--binary", required=True,
                        help="Path to the slave firmware .bin file")
    args = parser.parse_args()

    fw_path = Path(args.binary)
    if not fw_path.exists():
        print(f"ERROR: firmware file not found: {fw_path}")
        return 1

    firmware = fw_path.read_bytes()
    if not firmware:
        print("ERROR: firmware file is empty")
        return 1

    print("=" * 60)
    print("UC2 CANopen OTA — serial -> master -> CAN")
    print("=" * 60)
    print(f"Port        : {args.port} @ {args.baud} baud")
    print(f"Firmware    : {fw_path.name}")

    ok = flash_via_master(args.port, args.baud, args.node, firmware)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
