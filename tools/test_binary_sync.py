#!/usr/bin/env python3
"""
Minimal Binary Sync Test
========================
Tests if ESP32 correctly receives binary sync bytes 0xAA 0x55.

This is to debug why the ESP32 receives ASCII escape sequences
instead of raw binary bytes.

Usage:
  python test_binary_sync.py         - Run full test
  python test_binary_sync.py --reset - Just send reset/abort command
  python test_binary_sync.py --sync  - Just test sync bytes
"""

import serial
import time
import json
import sys
import struct
import zlib

PORT = "/dev/cu.SLAB_USBtoUART"
BAUD = 921600

def drain_serial(ser, label=""):
    """Read and display any pending data"""
    time.sleep(0.1)
    if ser.in_waiting:
        data = ser.read(ser.in_waiting)
        print(f"  [{label}] Drained {len(data)} bytes: {data[:100]}...")
        return data
    return b''

def send_json_command(ser, cmd_dict, wait_response=True):
    """Send JSON command and optionally wait for response"""
    json_str = json.dumps(cmd_dict, separators=(',', ':'))
    tx_data = (json_str + '\n').encode()
    
    print(f"  TX JSON: {json_str}")
    ser.write(tx_data)
    ser.flush()
    
    if wait_response:
        time.sleep(0.5)
        response = ser.read(ser.in_waiting or 2048)
        print(f"  RX: {response}")
        return response
    return None

def send_abort_command(ser):
    """Send abort command to exit binary mode"""
    print("\nSending ABORT command...")
    
    # Try JSON abort first
    send_json_command(ser, {
        "task": "/can_ota",
        "canid": 11,
        "action": "abort"
    })
    
    # Also send binary ABORT packet
    abort_packet = bytes([
        0xAA, 0x55,  # Sync
        0x10,        # CMD_ABORT
        0x00, 0x00,  # Chunk (not used)
        0x00, 0x00   # Size (not used)
    ])
    # Add dummy CRC
    abort_packet += bytes([0x00, 0x00, 0x00, 0x00])
    # Calculate checksum
    checksum = 0
    for b in abort_packet:
        checksum ^= b
    abort_packet += bytes([checksum])
    
    print(f"  TX Binary ABORT: {abort_packet.hex()}")
    ser.write(abort_packet)
    ser.flush()
    
    time.sleep(0.5)
    drain_serial(ser, "after abort")

def build_data_packet(chunk_index: int, data: bytes) -> bytes:
    """Build a complete DATA packet"""
    crc32 = zlib.crc32(data) & 0xFFFFFFFF
    
    # Header: sync(2) + cmd(1) + chunk(2) + size(2)
    header = bytes([0xAA, 0x55, 0x01])  # Sync + CMD_DATA
    header += struct.pack('>H', chunk_index)  # Big endian chunk
    header += struct.pack('>H', len(data))    # Big endian size
    
    # CRC32 little endian
    crc_bytes = struct.pack('<I', crc32)
    
    # Combine header + CRC + data
    packet_body = header + crc_bytes + data
    
    # XOR checksum
    checksum = 0
    for b in packet_body:
        checksum ^= b
    
    return packet_body + bytes([checksum])

def test_sync_only():
    """Just test if sync bytes are received correctly"""
    print("=" * 60)
    print("SYNC BYTE TEST ONLY")
    print("=" * 60)
    
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    time.sleep(0.3)
    drain_serial(ser, "initial")
    
    # Enter binary mode
    print("\n[1] Entering binary mode...")
    send_json_command(ser, {
        "task": "/can_ota", "canid": 11, "action": "start",
        "firmware_size": 1024, "total_chunks": 1,
        "chunk_size": 1024, "md5": "test", "binary_mode": 1, "qid": 1
    })
    
    time.sleep(0.5)
    send_json_command(ser, {
        "task": "/can_ota", "canid": 11, "action": "binary_start"
    }, wait_response=False)
    
    time.sleep(1)
    drain_serial(ser, "after binary_start")
    
    # Now send just the sync bytes
    print("\n[2] Sending ONLY sync bytes 0xAA 0x55...")
    sync = bytes([0xAA, 0x55])
    print(f"  Bytes to send: {[hex(b) for b in sync]}")
    print(f"  As hex string: {sync.hex()}")
    
    written = ser.write(sync)
    ser.flush()
    print(f"  Wrote {written} bytes")
    
    # Wait and see if ESP32 reports receiving them
    time.sleep(2)
    response = drain_serial(ser, "response")
    
    # Send abort
    send_abort_command(ser)
    
    ser.close()
    print("\nDone!")

def test_full():
    """Full binary protocol test"""
    print("=" * 60)
    print("FULL BINARY PROTOCOL TEST")
    print("=" * 60)
    print(f"\nPort: {PORT}")
    print(f"Baud: {BAUD}")
    
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    time.sleep(0.3)
    drain_serial(ser, "initial")
    
    # Verify serial port configuration
    print(f"\nSerial config:")
    print(f"  Port: {ser.port}")
    print(f"  Baudrate: {ser.baudrate}")
    print(f"  Bytesize: {ser.bytesize}")
    print(f"  Parity: {ser.parity}")
    print(f"  Stopbits: {ser.stopbits}")
    
    # Step 1: JSON START
    print("\n[1] Sending JSON START...")
    send_json_command(ser, {
        "task": "/can_ota", "canid": 11, "action": "start",
        "firmware_size": 4, "total_chunks": 1,
        "chunk_size": 1024, "md5": "deadbeef", "binary_mode": 1, "qid": 1
    })
    
    time.sleep(0.5)
    
    # Step 2: JSON binary_start
    print("\n[2] Sending JSON binary_start...")
    send_json_command(ser, {
        "task": "/can_ota", "canid": 11, "action": "binary_start"
    }, wait_response=False)
    
    time.sleep(1)
    drain_serial(ser, "after binary_start")
    
    # Step 3: Send binary DATA packet
    print("\n[3] Sending binary DATA packet...")
    
    # Small test data
    test_data = bytes([0xDE, 0xAD, 0xBE, 0xEF])
    packet = build_data_packet(0, test_data)
    
    print(f"  Packet ({len(packet)} bytes):")
    print(f"  Hex: {packet.hex()}")
    print(f"  Bytes: {[hex(b) for b in packet]}")
    
    # CRITICAL: Verify we're actually sending bytes, not strings
    print(f"\n  === TYPE VERIFICATION ===")
    print(f"  packet type: {type(packet)}")
    print(f"  Is bytes?: {isinstance(packet, bytes)}")
    print(f"  First byte value: {packet[0]} (should be 170 = 0xAA)")
    print(f"  First byte type: {type(packet[0])}")
    
    # Show breakdown
    print(f"\n  Breakdown:")
    print(f"    [0-1]   SYNC:    {packet[0]:02X} {packet[1]:02X}")
    print(f"    [2]     CMD:     {packet[2]:02X} (0x01=DATA)")
    print(f"    [3-4]   CHUNK:   {packet[3]:02X} {packet[4]:02X} = {(packet[3]<<8)|packet[4]}")
    print(f"    [5-6]   SIZE:    {packet[5]:02X} {packet[6]:02X} = {(packet[5]<<8)|packet[6]}")
    print(f"    [7-10]  CRC32:   {packet[7]:02X} {packet[8]:02X} {packet[9]:02X} {packet[10]:02X}")
    print(f"    [11-14] DATA:    {' '.join(f'{b:02X}' for b in packet[11:15])}")
    print(f"    [15]    CHKSUM:  {packet[15]:02X}")
    
    # Verify serial write method
    print(f"\n  === SERIAL WRITE TEST ===")
    print(f"  ser.write expects: bytes or bytearray")
    print(f"  We're sending: {type(packet)}")
    
    # Write packet - capture return value
    written = ser.write(packet)
    ser.flush()
    print(f"\n  Wrote {written} bytes to serial (expected {len(packet)})")
    
    if written != len(packet):
        print(f"  WARNING: Bytes written mismatch! Expected {len(packet)}, got {written}")
    
    # Wait for response
    time.sleep(2)
    response = ser.read(ser.in_waiting or 1024)
    
    print(f"\n[4] Response from ESP32:")
    print(f"  Raw ({len(response)} bytes): {response}")
    if response:
        print(f"  Hex: {response.hex()}")
        
        # Look for ACK/NAK
        for i in range(len(response) - 6):
            if response[i] == 0xAA and response[i+1] == 0x55:
                cmd = response[i+2]
                status = response[i+3]
                chunk = (response[i+4] << 8) | response[i+5]
                print(f"\n  Found binary response at offset {i}:")
                print(f"    Cmd:    0x{cmd:02X} ({'ACK' if cmd == 0x06 else 'NAK' if cmd == 0x07 else 'UNKNOWN'})")
                print(f"    Status: 0x{status:02X}")
                print(f"    Chunk:  {chunk}")
    
    # Cleanup
    print("\n[5] Cleanup...")
    send_abort_command(ser)
    
    ser.close()
    print("\nDone!")

def reset_only():
    """Just reset/abort any ongoing OTA"""
    print("=" * 60)
    print("RESET/ABORT ONLY")
    print("=" * 60)
    
    ser = serial.Serial(PORT, BAUD, timeout=0.5)
    time.sleep(0.3)
    drain_serial(ser, "initial")
    
    send_abort_command(ser)
    
    # Also send a simple query to verify ESP32 is responsive
    print("\nVerifying ESP32 is responsive...")
    response = send_json_command(ser, {"task": "/state_get"})
    
    ser.close()
    print("\nDone!")

def main():
    if len(sys.argv) > 1:
        if sys.argv[1] == "--reset":
            reset_only()
        elif sys.argv[1] == "--sync":
            test_sync_only()
        elif sys.argv[1] == "--help":
            print(__doc__)
        else:
            print(f"Unknown option: {sys.argv[1]}")
            print("Use --reset, --sync, or no argument for full test")
    else:
        test_full()

if __name__ == "__main__":
    main()
