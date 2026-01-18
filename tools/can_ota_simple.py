#!/usr/bin/env python3
"""
Simple CAN OTA Binary Upload
============================
Simplified version of can_ota_test.py following test_binary_sync.py style.
Uploads firmware to ESP32 slave via CAN bus through the master.

Usage:
  python can_ota_simple.py                    - Upload default firmware
  python can_ota_simple.py --firmware file.bin - Upload specific firmware
  python can_ota_simple.py --test             - Send only 3 chunks for testing
"""

import serial
import time
import json
import sys
import struct
import zlib
import hashlib
from pathlib import Path

# ============================================================================
# CONFIGURATION
# ============================================================================
PORT = "/dev/cu.SLAB_USBtoUART"
BAUD = 921600
CAN_ID = 11
CHUNK_SIZE = 1024

# Default firmware path
FIRMWARE = "/Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST/binaries/latest/esp32_seeed_xiao_esp32s3_can_slave_motor.bin"
TEST = False  # If True, only send 3 chunks for testing

# Protocol constants
SYNC_1 = 0xAA
SYNC_2 = 0x55
CMD_DATA = 0x01
CMD_ACK = 0x06
CMD_NAK = 0x07
CMD_END = 0x0F
CMD_ABORT = 0x10

# Timeouts - increased because master now waits for slave ACK over CAN
ACK_TIMEOUT = 20.0  # seconds - master waits up to 2s for slave ACK

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def drain_serial(ser, label="", verbose=True):
    """Read and display any pending data"""
    time.sleep(0.05)
    data = b''
    while ser.in_waiting:
        data += ser.read(ser.in_waiting)
        time.sleep(0.01)
    if data and verbose:
        print(f"  [{label}] Drained {len(data)} bytes")
    return data

def send_json(ser, cmd_dict, wait_response=True, timeout=2.0):
    """Send JSON command and optionally wait for response"""
    json_str = json.dumps(cmd_dict, separators=(',', ':'))
    tx_data = (json_str + '\n').encode()
    
    print(f"  TX: {json_str[:80]}{'...' if len(json_str) > 80 else ''}")
    ser.write(tx_data)
    ser.flush()
    
    if wait_response:
        time.sleep(0.3)
        start = time.time()
        response = b''
        while time.time() - start < timeout:
            if ser.in_waiting:
                response += ser.read(ser.in_waiting)
                if b'{"success":' in response or b'{"error":' in response:
                    break
            time.sleep(0.05)
        print(f"  RX: {len(response)} bytes")
        return response
    return None

def build_data_packet(chunk_index: int, data: bytes) -> bytes:
    """Build a complete DATA packet"""
    crc32 = zlib.crc32(data) & 0xFFFFFFFF
    
    # Header: sync(2) + cmd(1) + chunk(2) + size(2)
    header = bytes([SYNC_1, SYNC_2, CMD_DATA])
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

def build_end_packet(total_chunks: int) -> bytes:
    """Build END packet"""
    header = bytes([SYNC_1, SYNC_2, CMD_END])
    header += struct.pack('>H', total_chunks)
    header += struct.pack('>H', 0)  # No data
    header += bytes([0, 0, 0, 0])   # No CRC needed
    
    checksum = 0
    for b in header:
        checksum ^= b
    return header + bytes([checksum])

def wait_for_ack(ser, timeout=ACK_TIMEOUT):
    """
    Wait for ACK/NAK response from ESP32.
    Returns: (success, status, chunk_index, raw_response)
    """
    start = time.time()
    buffer = bytearray()
    
    while time.time() - start < timeout:
        if ser.in_waiting:
            new_data = ser.read(ser.in_waiting)
            print(new_data)
            buffer.extend(new_data)
            
            # Search for sync bytes in buffer
            i = 0
            while i < len(buffer) - 6:
                if buffer[i] == SYNC_1 and buffer[i+1] == SYNC_2:
                    cmd = buffer[i+2]
                    status = buffer[i+3]
                    chunk = (buffer[i+4] << 8) | buffer[i+5]
                    checksum = buffer[i+6]
                    
                    # Verify checksum
                    calc_checksum = 0
                    for b in buffer[i:i+6]:
                        calc_checksum ^= b
                    
                    if calc_checksum == checksum:
                        if cmd == CMD_ACK:
                            return (True, status, chunk, bytes(buffer))
                        elif cmd == CMD_NAK:
                            return (False, status, chunk, bytes(buffer))
                i += 1
        else:
            time.sleep(0.001)
    
    return (False, 0xFF, 0, bytes(buffer))

def send_abort(ser):
    """Send abort command"""
    print("\n  Sending ABORT...")
    send_json(ser, {"task": "/can_ota", "canid": CAN_ID, "action": "abort"}, wait_response=False)
    time.sleep(0.5)
    drain_serial(ser, "abort")

# ============================================================================
# MAIN UPLOAD FUNCTION
# ============================================================================

def upload_firmware(firmware_path: str, test_mode: bool = False):
    """Upload firmware using simple binary protocol"""
    
    # Load firmware
    firmware_path = Path(firmware_path)
    if not firmware_path.exists():
        print(f"ERROR: Firmware not found: {firmware_path}")
        return False
    
    firmware_data = firmware_path.read_bytes()
    firmware_size = len(firmware_data)
    md5_hash = hashlib.md5(firmware_data).hexdigest()
    
    # Calculate chunks
    num_chunks = (firmware_size + CHUNK_SIZE - 1) // CHUNK_SIZE
    
    if test_mode:
        num_chunks = min(3, num_chunks)  # Only send 3 chunks for testing
    
    print("=" * 70)
    print("CAN OTA Simple Binary Upload")
    print("=" * 70)
    print(f"Port:      {PORT}")
    print(f"Baud:      {BAUD}")
    print(f"CAN ID:    {CAN_ID}")
    print(f"Firmware:  {firmware_path.name}")
    print(f"Size:      {firmware_size:,} bytes")
    print(f"Chunks:    {num_chunks} (of {CHUNK_SIZE} bytes each)")
    print(f"MD5:       {md5_hash}")
    print(f"Test mode: {test_mode}")
    print("=" * 70)
    
    # Open serial
    print(f"\n[1] Opening serial port...")
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
    time.sleep(0.3)
    drain_serial(ser, "initial")
    
    try:
        # Step 2: Send JSON START command
        print(f"\n[2] Sending START command...")
        response = send_json(ser, {
            "task": "/can_ota",
            "canid": CAN_ID,
            "action": "start",
            "firmware_size": firmware_size,
            "total_chunks": num_chunks,
            "chunk_size": CHUNK_SIZE,
            "md5": md5_hash,
            "binary_mode": 1,
            "qid": 1
        })
        
        if b'"success":true' not in response:
            print("  ERROR: START command failed!")
            print(f"  Response: {response}")
            return False
        print("  ✓ Slave initialized")
        time.sleep(0.5)
        
        # Step 3: Enter binary mode
        print(f"\n[3] Entering binary mode...")
        send_json(ser, {
            "task": "/can_ota",
            "canid": CAN_ID,
            "action": "binary_start"
        }, wait_response=False)
        
        # Wait for ESP32 to switch to binary mode
        time.sleep(1.0)
        
        # Wait for initial ACK
        print("  Waiting for binary mode ACK...")
        success, status, chunk, raw = wait_for_ack(ser, timeout=3.0)
        if success:
            print(f"  ✓ Binary mode active (status={status})")
        else:
            print(f"  WARNING: No ACK received, continuing anyway...")
            # Drain any remaining data
            drain_serial(ser, "post-binary-start")
        
        # Step 4: Upload chunks
        print(f"\n[4] Uploading {num_chunks} chunks...")
        
        for chunk_idx in range(num_chunks):
            # Get chunk data
            start_pos = chunk_idx * CHUNK_SIZE
            end_pos = min(start_pos + CHUNK_SIZE, firmware_size)
            chunk_data = firmware_data[start_pos:end_pos]
            
            # Pad last chunk if necessary
            if len(chunk_data) < CHUNK_SIZE:
                chunk_data = chunk_data + bytes(CHUNK_SIZE - len(chunk_data))
            
            # Build packet
            packet = build_data_packet(chunk_idx, chunk_data)
            
            # Send with retries
            max_retries = 5
            chunk_sent = False
            
            for retry in range(max_retries):
                # Clear input buffer before sending
                ser.reset_input_buffer()
                
                # Send packet
                ser.write(packet)
                ser.flush()
                
                # Progress indicator
                progress = (chunk_idx + 1) / num_chunks * 100
                print(f"\r  Chunk {chunk_idx}/{num_chunks-1} ({progress:.1f}%) ", end="")
                
                # Wait for ACK
                success, status, resp_chunk, raw = wait_for_ack(ser)
                
                if success and status == 0:
                    chunk_sent = True
                    if chunk_idx < 3 or chunk_idx == num_chunks - 1:
                        print(f"✓ ACK")
                    break
                else:
                    error_msg = {
                        0x01: "SYNC_ERR",
                        0x02: "CHECKSUM_ERR", 
                        0x03: "CRC_ERR",
                        0x04: "SEQ_ERR",
                        0x05: "SIZE_ERR",
                        0x06: "TIMEOUT",
                        0x07: "RELAY_ERR",
                        0xFF: "NO_RESPONSE"
                    }.get(status, f"ERR_{status}")
                    
                    print(f"✗ {error_msg} (retry {retry+1}/{max_retries})")
                    
                    # Show some debug info on errors
                    if raw and len(raw) > 0:
                        # Check if there's text (log messages) in response
                        text_part = raw.split(b'\xaa\x55')[0] if b'\xaa\x55' in raw else raw
                        if text_part and len(text_part) > 0:
                            try:
                                print(f"    Debug: {text_part[:100].decode('utf-8', errors='replace')}")
                            except:
                                pass
                    
                    time.sleep(0.1 * (retry + 1))  # Exponential backoff
            
            if not chunk_sent:
                print(f"\n  ERROR: Failed to send chunk {chunk_idx} after {max_retries} retries")
                send_abort(ser)
                return False
        
        print()  # New line after progress
        
        # Step 5: Send END packet
        print(f"\n[5] Sending END packet...")
        end_packet = build_end_packet(num_chunks)
        ser.write(end_packet)
        ser.flush()
        
        success, status, chunk, raw = wait_for_ack(ser, timeout=10.0)
        if success:
            print(f"  ✓ END acknowledged")
        else:
            print(f"  WARNING: No END acknowledgement")
        
        # Step 6: Verify (optional)
        print(f"\n[6] Verifying...")
        time.sleep(0.5)
        
        # Exit binary mode first
        send_abort(ser)  # This will exit binary mode
        time.sleep(0.5)
        
        # Try to get status
        drain_serial(ser, "pre-verify")
        response = send_json(ser, {
            "task": "/can_ota",
            "canid": CAN_ID,
            "action": "status"
        })
        print(f"  Status: {response[:200] if response else 'No response'}")
        
        print("\n" + "=" * 70)
        print("UPLOAD COMPLETE!")
        if test_mode:
            print("(Test mode - only first 3 chunks sent)")
        print("=" * 70)
        
        return True
        
    except KeyboardInterrupt:
        print("\n\nInterrupted by user!")
        send_abort(ser)
        return False
        
    except Exception as e:
        print(f"\n  ERROR: {e}")
        import traceback
        traceback.print_exc()
        send_abort(ser)
        return False
        
    finally:
        ser.close()
        print("  Serial closed")

# ============================================================================
# MAIN
# ============================================================================

def main():

    # Update globals
    #global PORT, BAUD, CAN_ID
    #PORT = args.port
    #BAUD = args.baud
    #CAN_ID = args.canid
    
    success = upload_firmware(FIRMWARE, test_mode=TEST)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
