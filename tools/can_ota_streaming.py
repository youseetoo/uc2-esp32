#!/usr/bin/env python3
"""
CAN OTA Streaming Upload
========================
High-speed firmware upload using windowed streaming protocol.
Significantly faster than chunk-by-chunk ACK protocol.

Speed comparison:
- Old protocol (ACK per 1KB): ~6 minutes for 1MB
- Streaming (ACK per 4KB): ~1.5-2 minutes for 1MB

Usage:
  python can_ota_streaming.py                    - Upload default firmware
  python can_ota_streaming.py --firmware file.bin - Upload specific firmware
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

# Streaming protocol constants
PAGE_SIZE = 4096       # 4KB pages (flash-aligned)
CHUNK_SIZE = 512       # 512 bytes per chunk within page
CHUNKS_PER_PAGE = PAGE_SIZE // CHUNK_SIZE  # 8 chunks per page

# Default firmware path
FIRMWARE = "/Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST/binaries/latest/esp32_seeed_xiao_esp32s3_can_slave_motor.bin"

# Protocol constants (must match CanOtaStreaming.h)
SYNC_1 = 0xAA
SYNC_2 = 0x55

# Streaming message types (0x70-0x76)
STREAM_START  = 0x70
STREAM_DATA   = 0x71
STREAM_ACK    = 0x72
STREAM_NAK    = 0x73
STREAM_FINISH = 0x74
STREAM_ABORT  = 0x75
STREAM_STATUS = 0x76

# Timeouts
PAGE_ACK_TIMEOUT = 10.0  # 10 seconds per page ACK
TOTAL_TIMEOUT = 300.0    # 5 minutes total

# Retry configuration
MAX_PAGE_RETRIES = 3     # Maximum retries per page
MAX_SESSION_RETRIES = 2  # Maximum full session restart attempts

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

def build_stream_data_packet(page_index: int, offset: int, seq: int, data: bytes) -> bytes:
    """
    Build a streaming DATA packet.
    
    Wire format:
    [SYNC_1][SYNC_2][STREAM_DATA][page_H][page_L][offset_H][offset_L][size_H][size_L][seq_H][seq_L][DATA...][CHECKSUM]
    """
    header = bytes([SYNC_1, SYNC_2, STREAM_DATA])
    header += struct.pack('>H', page_index)    # Page index (big endian)
    header += struct.pack('>H', offset)         # Offset within page
    header += struct.pack('>H', len(data))      # Data size
    header += struct.pack('>H', seq)            # Sequence number
    
    packet_body = header + data
    
    # XOR checksum
    checksum = 0
    for b in packet_body:
        checksum ^= b
    
    return packet_body + bytes([checksum])

def build_stream_finish_packet(md5_bytes: bytes) -> bytes:
    """Build STREAM_FINISH packet with MD5 hash"""
    header = bytes([SYNC_1, SYNC_2, STREAM_FINISH])
    packet_body = header + md5_bytes
    
    checksum = 0
    for b in packet_body:
        checksum ^= b
    
    return packet_body + bytes([checksum])

def wait_for_stream_ack(ser, expected_page: int, timeout: float = PAGE_ACK_TIMEOUT):
    """
    Wait for STREAM_ACK response.
    Returns: (success, last_complete_page, bytes_received, raw_response)
    """
    start = time.time()
    buffer = bytearray()
    
    while time.time() - start < timeout:
        if ser.in_waiting:
            new_data = ser.read(ser.in_waiting)
            buffer.extend(new_data)
            
            # Check for log messages indicating success (for final ACK)
            try:
                text = bytes(buffer).decode('utf-8', errors='replace')
                if "OTA COMPLETE" in text or "Rebooting" in text:
                    return (True, expected_page, 0, bytes(buffer))
            except:
                pass
            
            # Search for STREAM_ACK response
            # Format: [SYNC][CMD][status][canId][lastPage_L][lastPage_H][bytes(4)][nextSeq(2)][reserved(2)]
            i = 0
            while i < len(buffer) - 15:
                if buffer[i] == SYNC_1 and buffer[i+1] == SYNC_2:
                    cmd = buffer[i+2]
                    
                    if cmd == STREAM_ACK:
                        status = buffer[i+3]
                        can_id = buffer[i+4]
                        last_page = buffer[i+5] | (buffer[i+6] << 8)  # Little endian
                        bytes_recv = struct.unpack('<I', bytes(buffer[i+7:i+11]))[0]
                        next_seq = buffer[i+11] | (buffer[i+12] << 8)
                        
                        if status == 0:  # CAN_OTA_OK
                            return (True, last_page, bytes_recv, bytes(buffer))
                        else:
                            print(f"  STREAM_ACK with error status: {status}")
                            return (False, last_page, bytes_recv, bytes(buffer))
                    
                    elif cmd == STREAM_NAK:
                        status = buffer[i+3]
                        can_id = buffer[i+4]
                        error_page = buffer[i+5] | (buffer[i+6] << 8)
                        missing_offset = buffer[i+7] | (buffer[i+8] << 8)
                        
                        print(f"  STREAM_NAK: status={status}, page={error_page}, offset={missing_offset}")
                        return (False, error_page, missing_offset, bytes(buffer))
                
                i += 1
        else:
            time.sleep(0.001)
    
    return (False, -1, 0, bytes(buffer))

def send_abort(ser):
    """Send abort command and wait for confirmation"""
    print("\n  Sending ABORT...")
    send_json(ser, {"task": "/can_ota_stream", "canid": CAN_ID, "action": "abort"}, wait_response=False)
    time.sleep(1.0)  # Give slave time to abort
    drain_serial(ser, "abort")
    time.sleep(0.5)  # Additional settle time

def reset_slave_state(ser):
    """Reset slave OTA state by sending abort and waiting"""
    print("  Resetting slave state...")
    send_abort(ser)
    # Send another abort to be sure
    time.sleep(0.5)
    send_json(ser, {"task": "/can_ota_stream", "canid": CAN_ID, "action": "abort"}, wait_response=False)
    time.sleep(1.0)
    drain_serial(ser, "reset")

def send_page_with_retry(ser, page_idx, page_data, seq_start, max_retries=MAX_PAGE_RETRIES):
    """
    Send a page with retry logic.
    Returns: (success, new_seq, acked_page, acked_bytes)
    """
    for retry in range(max_retries):
        seq = seq_start
        
        # Send all chunks for this page
        for chunk_idx in range(CHUNKS_PER_PAGE):
            chunk_start = chunk_idx * CHUNK_SIZE
            chunk_end = chunk_start + CHUNK_SIZE
            chunk_data = page_data[chunk_start:chunk_end]
            offset = chunk_idx * CHUNK_SIZE
            
            packet = build_stream_data_packet(page_idx, offset, seq, chunk_data)
            ser.write(packet)
            seq += 1
        
        ser.flush()
        
        # Wait for ACK
        success, acked_page, acked_bytes, raw = wait_for_stream_ack(ser, page_idx)
        
        if success:
            return (True, seq, acked_page, acked_bytes)
        
        if retry < max_retries - 1:
            print(f" [RETRY {retry+1}/{max_retries}]", end="")
            time.sleep(0.5)  # Small delay before retry
            drain_serial(ser, "retry", verbose=False)
    
    return (False, seq, -1, 0)

# ============================================================================
# MAIN UPLOAD FUNCTION
# ============================================================================

def upload_firmware_streaming(firmware_path: str, session_retry: int = 0):
    """Upload firmware using streaming protocol (4KB page-based)"""
    
    # Load firmware
    firmware_path = Path(firmware_path)
    if not firmware_path.exists():
        print(f"ERROR: Firmware not found: {firmware_path}")
        return False
    
    firmware_data = firmware_path.read_bytes()
    firmware_size = len(firmware_data)
    md5_hex = hashlib.md5(firmware_data).hexdigest()
    md5_bytes = bytes.fromhex(md5_hex)
    
    # Calculate pages
    num_pages = (firmware_size + PAGE_SIZE - 1) // PAGE_SIZE
    
    if session_retry == 0:
        print("=" * 70)
        print("CAN OTA STREAMING Upload")
        print("=" * 70)
        print(f"Port:       {PORT}")
        print(f"Baud:       {BAUD}")
        print(f"CAN ID:     {CAN_ID}")
        print(f"Firmware:   {firmware_path.name}")
        print(f"Size:       {firmware_size:,} bytes")
        print(f"Pages:      {num_pages} (of {PAGE_SIZE} bytes each)")
        print(f"Chunks/pg:  {CHUNKS_PER_PAGE} (of {CHUNK_SIZE} bytes each)")
        print(f"MD5:        {md5_hex}")
        print("=" * 70)
        
        estimated_time = num_pages * 0.5  # ~0.5 seconds per page
        print(f"Estimated time: {estimated_time:.0f} seconds ({estimated_time/60:.1f} minutes)")
        print("=" * 70)
    else:
        print(f"\n{'='*70}")
        print(f"SESSION RETRY {session_retry}/{MAX_SESSION_RETRIES}")
        print(f"{'='*70}")

    # Open serial
    print(f"\n[1] Opening serial port...")
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
    time.sleep(0.3)
    drain_serial(ser, "initial")
    
    # Reset slave state before starting (especially important on retry)
    if session_retry > 0:
        reset_slave_state(ser)
    
    send_json(ser, {"task": "/can_act", "debug": 1})

    
    start_time = time.time()
    
    try:
        # Step 2: Send JSON START command for streaming mode
        print(f"\n[2] Sending STREAM_START command...")
        stream_start_cmd = {
            "task": "/can_ota_stream",
            "canid": CAN_ID,
            "action": "start",
            "firmware_size": firmware_size,
            "page_size": PAGE_SIZE,
            "chunk_size": CHUNK_SIZE,
            "md5": md5_hex,
            "qid": 1
        }
        response = send_json(ser, stream_start_cmd)
        print(stream_start_cmd)
        #
        # {"task": "/can_ota_stream", "canid": 11, "action": "start", "firmware_size": 876784, "page_size": 4096, "chunk_size": 512, "md5": "43ba96b4d18c010201762b840476bf83", "qid": 1}
        if response and  str(response).find("success")<=0:
            print("  ERROR: STREAM_START command failed!")
            print(f"  Response: {response}")
            return False
        print("  ✓ Streaming session started")
        '''b\'[326584][I][CanOtaStreaming.cpp:427] actFromJsonStreaming(): Target CAN ID: 11\\r\\n[326586][I][CanOtaStreaming.cpp:456] actFromJsonStreaming(): Stream OTA START to CAN ID 11: size=876784, page_size=4096, chunk_size=512\\r\\n[326588][I][CanOtaStreaming.cpp:464] actFromJsonStreaming(): Relaying STREAM_START to slave 0x0B\\r\\n[326590][I][CanOtaStreaming.cpp:653] startStreamToSlave(): Starting stream to slave 0x0B: 876784 bytes, 215 pages\\r\\n[326968][I][CanOtaStreaming.cpp:622] handleSlaveStreamResponse(): Slave STREAM_ACK: page=65535, bytes=0, nextSeq=0\\r\\n++\\n{"success":true,"qid":1}\\n--\\n\\x00\''''
        # Wait for slave to initialize
        time.sleep(0.5)
        
        # Step 3: Stream pages
        print(f"\n[3] Streaming {num_pages} pages...")
        
        seq = 0  # Global sequence counter
        bytes_sent = 0
        failed_pages = 0
        
        for page_idx in range(num_pages):
            page_start = page_idx * PAGE_SIZE
            page_end = min(page_start + PAGE_SIZE, firmware_size)
            page_data = firmware_data[page_start:page_end]
            
            # Pad last page if necessary
            if len(page_data) < PAGE_SIZE:
                page_data = page_data + bytes(PAGE_SIZE - len(page_data))
            
            # Progress display
            progress = (page_idx + 1) / num_pages * 100
            elapsed = time.time() - start_time
            speed = bytes_sent / elapsed / 1024 if elapsed > 0 else 0
            print(f"\r  Page {page_idx+1}/{num_pages} ({progress:.1f}%) - {speed:.1f} KB/s ", end="")
            
            # Send page with retry logic
            success, seq, acked_page, acked_bytes = send_page_with_retry(
                ser, page_idx, page_data, seq
            )
            
            bytes_sent += PAGE_SIZE
            
            if not success:
                failed_pages += 1
                print(f"\n  ERROR: Page {page_idx} failed after {MAX_PAGE_RETRIES} retries!")
                
                # Check if we should retry the whole session
                if session_retry < MAX_SESSION_RETRIES:
                    print(f"  Attempting session restart...")
                    send_abort(ser)
                    ser.close()
                    time.sleep(2.0)  # Give slave time to recover
                    return upload_firmware_streaming(firmware_path, session_retry + 1)
                else:
                    send_abort(ser)
                    return False
            
            if page_idx % 10 == 0:
                print(f"✓ ACK (page={acked_page}, bytes={acked_bytes})")
        
        print(f"\n  All pages sent!")
        
        # Step 4: Send FINISH command
        print(f"\n[4] Sending STREAM_FINISH...")
        finish_packet = build_stream_finish_packet(md5_bytes)
        ser.write(finish_packet)
        ser.flush()
        
        # Wait for final ACK (MD5 verification takes time)
        print("  Waiting for MD5 verification...")
        success, _, _, raw = wait_for_stream_ack(ser, num_pages - 1, timeout=30.0)
        
        # Check for success OR reboot message (device may reboot before sending final ACK)
        if not success:
            # Check if raw response contains reboot indicator
            try:
                raw_text = raw.decode('utf-8', errors='replace') if raw else ""
                if "Rebooting" in raw_text or "OTA COMPLETE" in raw_text:
                    print("  Device is rebooting - OTA successful!")
                    success = True
            except:
                pass
        
        if not success:
            print("  ERROR: Final verification failed!")
            return False
        
        elapsed = time.time() - start_time
        speed = firmware_size / elapsed / 1024
        
        print(f"\n{'='*70}")
        print(f"SUCCESS! Firmware uploaded in {elapsed:.1f} seconds ({speed:.1f} KB/s)")
        print(f"{'='*70}")
        
        return True
        
    except KeyboardInterrupt:
        print("\n\nInterrupted by user!")
        send_abort(ser)
        return False
        
    except Exception as e:
        print(f"\n\nERROR: {e}")
        send_abort(ser)
        return False
        
    finally:
        print("\n  Serial closed")
        ser.close()

# ============================================================================
# MAIN
# ============================================================================

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="CAN OTA Streaming Upload")
    parser.add_argument("--firmware", "-f", default=FIRMWARE, help="Firmware file path")
    parser.add_argument("--port", "-p", default=PORT, help="Serial port")
    parser.add_argument("--canid", "-c", type=int, default=CAN_ID, help="Target CAN ID")
    args = parser.parse_args()
    
    PORT = args.port
    CAN_ID = args.canid
    
    success = upload_firmware_streaming(args.firmware)
sys.exit(0 if success else 1)
