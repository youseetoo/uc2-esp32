#!/usr/bin/env python3
"""
UC2 CAN Bus Sniffer
====================
Listens to all CAN traffic via a Waveshare USB-CAN-A adapter and prints every
frame to stdout with a human-readable decode of known UC2 CANopen PDO types.

Adjust SERIAL_PORT and CAN_BITRATE at the bottom to match your setup.

Requirements:
    pip install pyserial python-can

Usage:
    python can_sniffer.py
"""

import struct
import time

import can

from waveshare_bus import WaveshareBus

# ============================================================================
# UC2 COB-ID constants (from ObjectDictionary.h)
# ============================================================================

COBID_NMT   = 0x000
COBID_SYNC  = 0x080

FC_TPDO1 = 0x180   # Motor status        (node → master)
FC_TPDO2 = 0x280   # Node status         (node → master)
FC_TPDO3 = 0x380   # Sensor data         (node → master)
FC_RPDO1 = 0x200   # Motor pos/speed     (master → node)
FC_RPDO2 = 0x300   # Axis cmd / laser    (master → node)
FC_RPDO3 = 0x400   # LED extra / qid     (master → node)
FC_TSDO  = 0x580   # SDO response        (node → master)
FC_RSDO  = 0x600   # SDO request         (master → node)
FC_HB    = 0x700   # Heartbeat / NMT err (node → master)

# NMT state names
NMT_STATES = {
    0x00: "INITIALIZING",
    0x04: "STOPPED",
    0x05: "OPERATIONAL",
    0x7F: "PRE-OPERATIONAL",
}

# Motor command names
MOTOR_CMDS = {0: "STOP", 1: "MOVE_REL", 2: "MOVE_ABS", 3: "HOME"}

# SDO command specifiers (cs field, bits [7:5])
SDO_CS = {
    0b001: "INIT_DOWNLOAD",
    0b010: "INIT_UPLOAD",
    0b011: "INIT_UPLOAD_RSP",
    0b100: "INIT_DOWNLOAD_RSP",
    0b101: "ABORT",
}

AXIS_LASER = 0xFF
AXIS_LED   = 0xFE


# ============================================================================
# Decode helpers
# ============================================================================

def _fc(cob_id: int) -> int:
    """Extract function code (top 4 bits, shifted)."""
    return cob_id & 0x780


def _node(cob_id: int) -> int:
    """Extract node ID (lower 7 bits)."""
    return cob_id & 0x07F


def decode_nmt(data: bytes) -> str:
    if len(data) < 2:
        return f"(too short: {data.hex()})"
    NMT_CMDS = {
        0x01: "START", 0x02: "STOP",
        0x80: "PRE-OPERATIONAL", 0x81: "RESET-NODE", 0x82: "RESET-COMM",
    }
    cmd_name = NMT_CMDS.get(data[0], f"0x{data[0]:02X}")
    target = f"node 0x{data[1]:02X}" if data[1] != 0 else "ALL nodes"
    return f"NMT  cmd={cmd_name}  target={target}"


def decode_heartbeat(node_id: int, data: bytes) -> str:
    if not data:
        return f"HB   node=0x{node_id:02X}  (empty)"
    state_name = NMT_STATES.get(data[0], f"0x{data[0]:02X}")
    return f"HB   node=0x{node_id:02X}  state={state_name}"


def decode_tpdo1(node_id: int, data: bytes) -> str:
    """TPDO1: int32 actual_pos, uint8 axis, uint8 is_running, int16 qid"""
    if len(data) < 8:
        return f"TPDO1 node=0x{node_id:02X}  (short: {data.hex()})"
    actual_pos, axis, is_running, qid = struct.unpack_from("<iBBh", data)
    return (f"TPDO1 node=0x{node_id:02X}  axis={axis}"
            f"  pos={actual_pos:+d}  running={bool(is_running)}  qid={qid}")


def decode_tpdo2(node_id: int, data: bytes) -> str:
    """TPDO2: uint8 caps, uint8 node_id, uint8 err, int16 temp, int16 enc, uint8 nmt"""
    if len(data) < 8:
        return f"TPDO2 node=0x{node_id:02X}  (short: {data.hex()})"
    caps, nid, err, temp, enc, nmt = struct.unpack_from("<BBBhhB", data)
    nmt_name = NMT_STATES.get(nmt, f"0x{nmt:02X}")
    cap_bits = []
    if caps & 0x01: cap_bits.append("MOTOR")
    if caps & 0x02: cap_bits.append("LASER")
    if caps & 0x04: cap_bits.append("LED")
    if caps & 0x08: cap_bits.append("ENCODER")
    caps_str = "|".join(cap_bits) if cap_bits else "none"
    return (f"TPDO2 node=0x{node_id:02X}  caps=[{caps_str}]"
            f"  err=0x{err:02X}  temp={temp/10:.1f}°C"
            f"  enc={enc}  nmt={nmt_name}")


def decode_tpdo3(node_id: int, data: bytes) -> str:
    """TPDO3: sensor data (layout device-specific, show raw)"""
    return f"TPDO3 node=0x{node_id:02X}  data={data.hex(' ')}"


def decode_rpdo1(node_id: int, data: bytes) -> str:
    """RPDO1: int32 target_pos, int32 speed"""
    if len(data) < 8:
        return f"RPDO1 node=0x{node_id:02X}  (short: {data.hex()})"
    target_pos, speed = struct.unpack_from("<ii", data)
    return f"RPDO1 node=0x{node_id:02X}  target_pos={target_pos:+d}  speed={speed}"


def decode_rpdo2(node_id: int, data: bytes) -> str:
    """RPDO2: uint8 axis, uint8 cmd, uint8 laser_id, uint16 laser_pwm, uint8 led_mode, uint8 r, uint8 g"""
    if len(data) < 7:
        return f"RPDO2 node=0x{node_id:02X}  (short: {data.hex()})"
    axis, cmd, laser_id, laser_pwm, led_mode, r, g = struct.unpack_from("<BBBHBBB", data)

    if axis == AXIS_LASER:
        return (f"RPDO2 node=0x{node_id:02X}  [LASER]"
                f"  ch={laser_id}  pwm={laser_pwm}")
    if axis == AXIS_LED:
        return (f"RPDO2 node=0x{node_id:02X}  [LED]"
                f"  mode={led_mode}  R={r}  G={g}")

    cmd_name = MOTOR_CMDS.get(cmd, f"0x{cmd:02X}")
    return (f"RPDO2 node=0x{node_id:02X}  [MOTOR]"
            f"  axis={axis}  cmd={cmd_name}"
            f"  laser_ch={laser_id}  laser_pwm={laser_pwm}")


def decode_rpdo3(node_id: int, data: bytes) -> str:
    """RPDO3: uint8 b, uint16 led_index, int16 qid, pad[3]"""
    if len(data) < 5:
        return f"RPDO3 node=0x{node_id:02X}  (short: {data.hex()})"
    b, led_index, qid = struct.unpack_from("<BHh", data)
    return (f"RPDO3 node=0x{node_id:02X}  [LED_EXTRA / QID]"
            f"  B={b}  led_index={led_index}  qid={qid}")


def decode_sdo(node_id: int, data: bytes, direction: str) -> str:
    """Decode SDO request or response (expedited, 4-byte data max)."""
    if len(data) < 4:
        return f"SDO  {direction} node=0x{node_id:02X}  (short: {data.hex()})"
    cs = (data[0] >> 5) & 0x07
    index = struct.unpack_from("<H", data, 1)[0]
    subindex = data[3]
    payload = data[4:] if len(data) > 4 else b""
    cs_name = SDO_CS.get(cs, f"cs=0x{cs:02X}")
    val_str = ""
    if payload:
        # Try to show numeric value
        if len(payload) == 4:
            val_str = f"  val={struct.unpack_from('<i', payload)[0]} / 0x{struct.unpack_from('<I', payload)[0]:08X}"
        elif len(payload) == 2:
            val_str = f"  val={struct.unpack_from('<h', payload)[0]}"
        elif len(payload) == 1:
            val_str = f"  val={payload[0]}"
    return (f"SDO  {direction} node=0x{node_id:02X}"
            f"  {cs_name}  idx=0x{index:04X} sub={subindex}{val_str}")


def decode_message(msg: can.Message) -> str:
    """Decode a CAN message to a human-readable string."""
    cob = msg.arbitration_id
    data = bytes(msg.data)
    fc = _fc(cob)
    node_id = _node(cob)

    if cob == COBID_NMT:
        detail = decode_nmt(data)
    elif cob == COBID_SYNC:
        detail = "SYNC"
    elif fc == FC_TPDO1:
        detail = decode_tpdo1(node_id, data)
    elif fc == FC_TPDO2:
        detail = decode_tpdo2(node_id, data)
    elif fc == FC_TPDO3:
        detail = decode_tpdo3(node_id, data)
    elif fc == FC_RPDO1:
        detail = decode_rpdo1(node_id, data)
    elif fc == FC_RPDO2:
        detail = decode_rpdo2(node_id, data)
    elif fc == FC_RPDO3:
        detail = decode_rpdo3(node_id, data)
    elif fc == FC_TSDO:
        detail = decode_sdo(node_id, data, "RSP ←")
    elif fc == FC_RSDO:
        detail = decode_sdo(node_id, data, "REQ →")
    elif fc == FC_HB:
        detail = decode_heartbeat(node_id, data)
    else:
        id_str = f"0x{cob:03X}" + (" [ext]" if msg.is_extended_id else "")
        detail = f"UNKNOWN  id={id_str}  data={data.hex(' ')}"

    ext_flag = "X" if msg.is_extended_id else " "
    rtr_flag = "R" if msg.is_remote_frame else " "
    ts = msg.timestamp or time.time()
    return f"[{ts:.6f}] {ext_flag}{rtr_flag} 0x{cob:03X}  DLC={len(data)}  {detail}"


# ============================================================================
# Sniffer main
# ============================================================================

def sniff(serial_port: str, can_bitrate: int, serial_baudrate: int = 2_000_000):
    """
    Open the Waveshare adapter and print every received CAN frame.

    Args:
        serial_port:     e.g. '/dev/ttyUSB0' or 'COM3'
        can_bitrate:     CAN bus speed in bits/s (must match the network)
        serial_baudrate: Serial interface speed (Waveshare default: 2 000 000)
    """
    bus = WaveshareBus(
        channel=serial_port,
        bitrate=can_bitrate,
        serial_baudrate=serial_baudrate,
    )

    print(f"CAN sniffer started — port={serial_port}  CAN={can_bitrate//1000}k")
    print("Press Ctrl-C to stop.\n")
    print(f"{'Timestamp':>16}  {'F':1} {'COB-ID':5}  {'DL':2}  Details")
    print("-" * 80)

    try:
        while True:
            msg = bus.recv(timeout=1.0)
            if msg is None:
                continue
            print(decode_message(msg), flush=True)
    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        bus.shutdown()


# ============================================================================
# Entry point — adjust these three values for your hardware
# ============================================================================

if __name__ == "__main__":
    SERIAL_PORT    = "/dev/cu.wchusbserial10"  # Waveshare USB-CAN-A port
    CAN_BITRATE    = 500_000                    # Must match the CAN network
    SERIAL_BAUDRATE = 2_000_000                 # Waveshare default

    sniff(SERIAL_PORT, CAN_BITRATE, SERIAL_BAUDRATE)
