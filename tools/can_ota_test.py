#!/usr/bin/env python3
"""
CAN OTA Binary Protocol Test Script
====================================
Standalone test script to verify binary OTA upload functionality.

This script performs OTA firmware update over CAN bus using a binary streaming protocol:
1. JSON commands for control (START, binary_start, END, VERIFY)
2. Raw binary packets for high-speed data transfer at 2Mbaud

IMPORTANT TIMING:
- The ESP32 switches baudrate IMMEDIATELY when it receives "binary_start"
- Python must switch baudrate right after sending "binary_start" (don't wait for response)
- The initial ACK is sent at 2Mbaud by ESP32

Debug output format:
- TX: shows bytes sent
- RX: shows bytes received

Author: UC2 Team
"""

import serial
import time
import json
import struct
import zlib
import hashlib
import sys
from pathlib import Path
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional, Tuple

# ============================================================================
# CONFIGURATION - Adjust these for your setup
# ============================================================================

# Serial port
DEFAULT_PORT = "/dev/cu.SLAB_USBtoUART"

# Target CAN ID of the slave device
DEFAULT_CAN_ID = 11

# Baudrates - MUST match ESP32 settings
NORMAL_BAUDRATE = 921600    # Normal serial baudrate (matches PinConfig BAUDRATE)
HIGH_BAUDRATE = 921600     # High-speed baudrate for binary transfer (matches BINARY_OTA_HIGH_BAUDRATE)

# Chunk size - must match ESP32 BINARY_OTA_MAX_CHUNK_SIZE  
CHUNK_SIZE = 1024

# Default firmware path
DEFAULT_FIRMWARE = "/Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST/binaries/latest/esp32_seeed_xiao_esp32s3_can_slave_motor.bin"

# Timeouts
ACK_TIMEOUT_SEC = 2.0
JSON_TIMEOUT_SEC = 10.0

# Max retries per chunk
MAX_RETRIES = 5

# Debug verbosity
DEBUG_VERBOSE = True  # Set to True to see all TX/RX bytes

# ============================================================================
# PROTOCOL CONSTANTS
# ============================================================================

# Sync bytes
SYNC_1 = 0xAA
SYNC_2 = 0x55

# Commands
CMD_DATA = 0x01
CMD_ACK = 0x06
CMD_NAK = 0x07
CMD_END = 0x0F
CMD_ABORT = 0x10
CMD_SWITCH_BAUD = 0x11


class OTAStatus(IntEnum):
    """OTA status/error codes"""
    OK = 0x00
    ERR_SYNC = 0x01
    ERR_CHECKSUM = 0x02
    ERR_CRC = 0x03
    ERR_SEQUENCE = 0x04
    ERR_SIZE = 0x05
    ERR_TIMEOUT = 0x06
    ERR_RELAY = 0x07
    ERR_SLAVE_NAK = 0x08


@dataclass
class OTAProgress:
    """Progress tracking"""
    bytes_sent: int
    bytes_total: int
    chunks_sent: int
    chunks_total: int
    elapsed_sec: float
    retries: int = 0
    
    @property
    def percent(self) -> float:
        return (self.bytes_sent / self.bytes_total * 100) if self.bytes_total > 0 else 0
    
    @property
    def speed_kbps(self) -> float:
        return (self.bytes_sent / 1024 / self.elapsed_sec) if self.elapsed_sec > 0 else 0


# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def log_tx(data: bytes, label: str = ""):
    """Log transmitted bytes"""
    if DEBUG_VERBOSE:
        hex_str = " ".join(f"{b:02X}" for b in data[:64])
        if len(data) > 64:
            hex_str += f" ... ({len(data)} bytes total)"
        print(f"  TX: [{label}] {hex_str}")


def log_rx(data: bytes, label: str = ""):
    """Log received bytes"""
    if DEBUG_VERBOSE:
        hex_str = " ".join(f"{b:02X}" for b in data[:64])
        if len(data) > 64:
            hex_str += f" ... ({len(data)} bytes total)"
        print(f"  RX: [{label}] {hex_str}")


def calculate_crc32(data: bytes) -> int:
    """Calculate CRC32 (same as ESP32 esp_crc32_le)"""
    return zlib.crc32(data) & 0xFFFFFFFF


def calculate_checksum(data: bytes) -> int:
    """Calculate XOR checksum of all bytes"""
    checksum = 0
    for b in data:
        checksum ^= b
    return checksum


def calculate_md5(data: bytes) -> str:
    """Calculate MD5 hash"""
    return hashlib.md5(data).hexdigest()


# ============================================================================
# PACKET BUILDING
# ============================================================================

def build_data_packet(chunk_index: int, data: bytes) -> bytes:
    """
    Build a binary data packet.
    
    Format: [0xAA][0x55][0x01][CHUNK_H][CHUNK_L][SIZE_H][SIZE_L][CRC32_LE(4)][DATA][CHECKSUM]
    
    - SYNC bytes: 0xAA 0x55
    - Command: 0x01 (DATA)
    - Chunk index: 16-bit big-endian
    - Data size: 16-bit big-endian
    - CRC32: 32-bit little-endian (of DATA only)
    - DATA: raw firmware bytes
    - CHECKSUM: XOR of all preceding bytes
    """
    crc32 = calculate_crc32(data)
    size = len(data)
    
    # Build header: SYNC(2) + CMD(1) + CHUNK(2) + SIZE(2) = 7 bytes
    header = bytes([SYNC_1, SYNC_2, CMD_DATA])
    header += struct.pack('>HH', chunk_index, size)  # Big-endian chunk and size
    
    # CRC32 in little-endian: 4 bytes
    crc_bytes = struct.pack('<I', crc32)
    
    # Combine: header + CRC + data
    packet_body = header + crc_bytes + data
    
    # Calculate checksum of entire packet body
    checksum = calculate_checksum(packet_body)
    
    return packet_body + bytes([checksum])


def build_end_packet() -> bytes:
    """Build END packet to signal transfer complete"""
    # Header: SYNC(2) + CMD(1) + CHUNK(2) + SIZE(2) = 7 bytes
    header = bytes([SYNC_1, SYNC_2, CMD_END, 0x00, 0x00, 0x00, 0x00])
    # Dummy CRC: 4 bytes
    header += bytes([0x00, 0x00, 0x00, 0x00])
    # Checksum
    checksum = calculate_checksum(header)
    return header + bytes([checksum])


def build_switch_baud_packet() -> bytes:
    """Build packet to switch back to normal baudrate"""
    header = bytes([SYNC_1, SYNC_2, CMD_SWITCH_BAUD, 0x00, 0x00, 0x00, 0x00])
    header += bytes([0x00, 0x00, 0x00, 0x00])  # Dummy CRC
    checksum = calculate_checksum(header)
    return header + bytes([checksum])


# ============================================================================
# MAIN UPLOADER CLASS
# ============================================================================

class CANOTAUploader:
    """CAN OTA Binary Protocol Uploader"""
    
    def __init__(self, port: str, normal_baud: int = NORMAL_BAUDRATE,
                 high_baud: int = HIGH_BAUDRATE, chunk_size: int = CHUNK_SIZE):
        self.port = port
        self.normal_baud = normal_baud
        self.high_baud = high_baud
        self.chunk_size = chunk_size
        self.ser: Optional[serial.Serial] = None
        self._in_binary_mode = False
    
    def open_serial(self, baudrate: int) -> bool:
        """Open or reconfigure serial port"""
        try:
            if self.ser and self.ser.is_open:
                if self.ser.baudrate != baudrate:
                    print(f"  Switching baudrate: {self.ser.baudrate} → {baudrate}")
                    self.ser.flush()
                    time.sleep(0.05)
                    self.ser.close()
                    time.sleep(0.1)
            
            self.ser = serial.Serial(
                port=self.port,
                baudrate=baudrate,
                timeout=0.1,
                write_timeout=2.0
            )
            # Clear any garbage
            time.sleep(0.1)
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            print(f"  Serial opened at {baudrate} baud")
            return True
            
        except serial.SerialException as e:
            print(f"  ERROR: Serial error: {e}")
            return False
    
    def close_serial(self):
        """Close serial port"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            self.ser = None
            print("  Serial closed")
    
    def send_json(self, command: dict, timeout: float = JSON_TIMEOUT_SEC) -> Optional[dict]:
        """Send JSON command and wait for JSON response"""
        json_str = json.dumps(command, separators=(',', ':'))
        tx_data = (json_str + '\n').encode()
        
        log_tx(tx_data, "JSON")
        
        self.ser.reset_input_buffer()
        self.ser.write(tx_data)
        self.ser.flush()
        
        # Wait for JSON response
        start = time.time()
        buffer = ""
        
        while time.time() - start < timeout:
            if self.ser.in_waiting:
                try:
                    raw = self.ser.read(self.ser.in_waiting)
                    log_rx(raw, "JSON")
                    chars = raw.decode('utf-8', errors='ignore')
                    buffer += chars
                    
                    # Look for complete JSON object
                    if '{' in buffer and '}' in buffer:
                        start_idx = buffer.find('{')
                        # Find matching closing brace
                        depth = 0
                        for i, c in enumerate(buffer[start_idx:], start_idx):
                            if c == '{':
                                depth += 1
                            elif c == '}':
                                depth -= 1
                                if depth == 0:
                                    try:
                                        result = json.loads(buffer[start_idx:i+1])
                                        return result
                                    except json.JSONDecodeError:
                                        pass
                except Exception as e:
                    print(f"  Warning: {e}")
            else:
                time.sleep(0.01)
        
        print(f"  Warning: No JSON response within {timeout}s")
        return None
    
    def wait_for_binary_ack(self, timeout: float = ACK_TIMEOUT_SEC) -> Tuple[bool, int, int]:
        """
        Wait for binary ACK/NAK response.
        
        Response format: [0xAA][0x55][CMD][STATUS][CHUNK_H][CHUNK_L][CHECKSUM]
        
        Returns: (success, status, chunk_index)
        """
        start = time.time()
        buffer = bytearray()
        
        while time.time() - start < timeout:
            if self.ser.in_waiting:
                new_data = self.ser.read(self.ser.in_waiting)
                buffer.extend(new_data)
                if DEBUG_VERBOSE and len(new_data) > 0:
                    # log_rx(bytes(new_data), "BIN")
                    print("  RX: [JSON] :", new_data)
                
                # Look for sync sequence and parse response
                while len(buffer) >= 7:  # Minimum response size
                    if buffer[0] == SYNC_1 and buffer[1] == SYNC_2:
                        cmd = buffer[2]
                        status = buffer[3]
                        chunk_index = (buffer[4] << 8) | buffer[5]
                        checksum = buffer[6]
                        
                        # Verify checksum
                        calc_checksum = calculate_checksum(bytes(buffer[:6]))
                        if calc_checksum != checksum:
                            print(f"    Checksum error: got 0x{checksum:02X}, expected 0x{calc_checksum:02X}")
                            buffer = buffer[1:]  # Skip one byte and retry
                            continue
                        
                        if cmd == CMD_ACK:
                            return (True, status, chunk_index)
                        elif cmd == CMD_NAK:
                            return (False, status, chunk_index)
                        else:
                            print(f"    Unknown response cmd: 0x{cmd:02X}")
                            buffer = buffer[7:]
                            continue
                    else:
                        # Skip non-sync byte
                        buffer = buffer[1:]
            else:
                time.sleep(0.001)
        
        return (False, OTAStatus.ERR_TIMEOUT, 0)
    
    def send_binary_chunk(self, chunk_index: int, data: bytes) -> bool:
        """Send binary chunk with retries, wait for ACK"""
        packet = build_data_packet(chunk_index, data)
        
        for retry in range(MAX_RETRIES):
            if DEBUG_VERBOSE and chunk_index < 3:  # Only log first few chunks
                log_tx(packet[:20], f"DATA chunk={chunk_index}")
            
            self.ser.reset_input_buffer()
            self.ser.write(packet)
            print(f"    Sent chunk {chunk_index}, waiting for ACK...: ", packet, ";")
            self.ser.flush()
            
            success, status, resp_chunk = self.wait_for_binary_ack()
            
            if success and status == OTAStatus.OK:
                return True
            
            # Handle errors
            if status == OTAStatus.ERR_SEQUENCE:
                print(f"    Sequence error: ESP expects chunk {resp_chunk}, we sent {chunk_index}")
            elif status == OTAStatus.ERR_TIMEOUT:
                print(f"    Timeout waiting for ACK (retry {retry + 1}/{MAX_RETRIES})")
            elif status == OTAStatus.ERR_CRC:
                print(f"    CRC error (retry {retry + 1}/{MAX_RETRIES})")
            elif status == OTAStatus.ERR_CHECKSUM:
                print(f"    Checksum error (retry {retry + 1}/{MAX_RETRIES})")
            elif status == OTAStatus.ERR_RELAY:
                print(f"    Relay to slave failed (retry {retry + 1}/{MAX_RETRIES})")
            else:
                print(f"    NAK: status={status}, chunk={resp_chunk} (retry {retry + 1}/{MAX_RETRIES})")
            
            # Exponential backoff
            time.sleep(0.05 * (retry + 1))
        
        return False
    
    def upload_firmware(self, canid: int, firmware_path: str) -> bool:
        """
        Upload firmware using binary protocol.
        
        Flow:
        1. Connect at normal baudrate (115200)
        2. Send JSON START command
        3. Send JSON binary_start command
        4. IMMEDIATELY switch to high baudrate (2Mbaud)
        5. Wait for binary ACK from ESP32
        6. Send binary DATA chunks
        7. Send binary END packet
        8. Switch back to normal baudrate
        9. Send JSON VERIFY command
        """
        firmware_path = Path(firmware_path)
        
        # Validate firmware
        if not firmware_path.exists():
            print(f"ERROR: Firmware not found: {firmware_path}")
            return False
        
        firmware_data = firmware_path.read_bytes()
        firmware_size = len(firmware_data)
        
        if firmware_size == 0:
            print("ERROR: Firmware is empty")
            return False
        
        md5_hash = calculate_md5(firmware_data)
        num_chunks = (firmware_size + self.chunk_size - 1) // self.chunk_size
        
        print("=" * 70)
        print("CAN OTA Binary Protocol - Test Upload")
        print("=" * 70)
        print(f"Port:          {self.port}")
        print(f"CAN ID:        {canid}")
        print(f"Firmware:      {firmware_path.name}")
        print(f"Size:          {firmware_size:,} bytes ({num_chunks} chunks of {self.chunk_size} bytes)")
        print(f"MD5:           {md5_hash}")
        print(f"Normal Baud:   {self.normal_baud}")
        print(f"High Baud:     {self.high_baud}")
        print("=" * 70)
        
        try:
            # Step 1: Open at normal baudrate
            print(f"\n[1/7] Connecting at {self.normal_baud} baud...")
            if not self.open_serial(self.normal_baud):
                return False
            
            # Drain any old data
            time.sleep(0.3)
            if self.ser.in_waiting:
                garbage = self.ser.read(self.ser.in_waiting)
                log_rx(garbage, "DRAIN")
            
            # Step 2: Send JSON START command
            
            json_message = {
                "task": "/can_ota",
                "canid": canid,
                "action": "start",
                "firmware_size": firmware_size,
                "total_chunks": num_chunks,
                "chunk_size": self.chunk_size,
                "md5": md5_hash,
                "binary_mode": int(True)
            }
            json_message = {"task":"/can_ota", "canid": 11, "action": "start", "firmware_size": 865056, "total_chunks": 845, "chunk_size": 1024, "md5": "dde1cd1477ca4f2461c2b1fe0dd621c9", "binary_mode": 1, "qid":1}
            print("\n[2/7] Sending JSON START command: ", json_message)
            response = self.send_json(json_message)
            
            if not response:
                print("  ERROR: No response from START command")
                return False
            
            if not response.get('success'):
                print(f"  ERROR: START failed: {response.get('error', 'Unknown')}")
                return False
            
            print("  ✓ Slave initialized for OTA")
            time.sleep(1)
            
            # Step 3: Send JSON binary_start command
            print(f"\n[3/7] Sending JSON binary_start command...")
            print("  NOTE: ESP32 will switch to 2Mbaud after receiving this")
            print("  ESP32 will wait 500ms before sending ACK to allow Python to switch")
            
            # Send the command - don't wait for response (ESP switches baudrate immediately)
            json_str = json.dumps({
                "task": "/can_ota",
                "canid": canid,
                "action": "binary_start"
            }, separators=(',', ':'))
            print("\n[3/7] Sending JSON binary_start command: ", json_str)
            tx_data = (json_str + '\n').encode()
            log_tx(tx_data, "JSON binary_start")
            print("  Sending binary_start command: ", tx_data)
            self.ser.write(tx_data)
            self.ser.flush()
            
            # Give ESP32 time to process command and start baudrate switch
            time.sleep(1)
            
            # Step 4: Switch Python to high baudrate
            print(f"\n[4/7] Switching Python to {self.high_baud} baud...")
            if self.high_baud != self.normal_baud:
                if not self.open_serial(self.high_baud):
                    print("  ERROR: Failed to switch baudrate")
                    return False
            
                print("  Python now at 2Mbaud, waiting for ESP32...")
                time.sleep(1)  # Give ESP32 time to also switch (it waits 500ms total)
                
            self._in_binary_mode = True
            # Step 5: Wait for binary ACK from ESP32
            print("\n[5/7] Waiting for binary mode ACK from ESP32...")
            success, status, chunk = self.wait_for_binary_ack(timeout=3.0)
            if success:
                print(f"  ✓ Binary mode active! (status={status}, chunk={chunk})")
            else:
                print(f"  WARNING: No initial ACK received (status={status})")
                print("  Continuing anyway - ESP32 might already be waiting for data...")
            
            # Step 6: Upload chunks
            print(f"\n[6/7] Uploading {num_chunks} chunks...")
            start_time = time.time()
            total_retries = 0
            
            for chunk_idx in range(num_chunks):
                # Extract chunk data
                chunk_start = chunk_idx * self.chunk_size
                chunk_end = min(chunk_start + self.chunk_size, firmware_size)
                chunk_data = firmware_data[chunk_start:chunk_end]
                
                # Send with retry
                if not self.send_binary_chunk(chunk_idx, chunk_data):
                    print(f"\n  ERROR: Failed to send chunk {chunk_idx} after {MAX_RETRIES} retries")
                    return False
                
                # Progress every 5% or at start/end
                if chunk_idx == 0 or chunk_idx == num_chunks - 1 or (chunk_idx + 1) % max(1, num_chunks // 20) == 0:
                    elapsed = time.time() - start_time
                    progress = OTAProgress(
                        bytes_sent=chunk_end,
                        bytes_total=firmware_size,
                        chunks_sent=chunk_idx + 1,
                        chunks_total=num_chunks,
                        elapsed_sec=elapsed,
                        retries=total_retries
                    )
                    print(f"  [{chunk_idx + 1}/{num_chunks}] {progress.percent:.1f}% | {progress.speed_kbps:.1f} KB/s")
            
            elapsed = time.time() - start_time
            speed = firmware_size / 1024 / elapsed if elapsed > 0 else 0
            print(f"\n  ✓ Upload complete: {elapsed:.1f}s, {speed:.1f} KB/s average")
            
            # Step 7: Send END and switch back to normal baudrate
            print(f"\n[7/7] Finalizing...")
            
            # Send END packet
            end_packet = build_end_packet()
            log_tx(end_packet, "END")
            self.ser.write(end_packet)
            self.ser.flush()
            time.sleep(0.1)
            
            # Send switch baudrate packet
            switch_packet = build_switch_baud_packet()
            log_tx(switch_packet, "SWITCH_BAUD")
            self.ser.write(switch_packet)
            self.ser.flush()
            time.sleep(0.2)
            
            # Switch back to normal baudrate
            self._in_binary_mode = False
            if not self.open_serial(self.normal_baud):
                print("  WARNING: Failed to switch back to normal baudrate")
            
            time.sleep(0.3)
            
            # Verify
            print("\n[8/7] Verifying firmware (bonus step)...")
            response = self.send_json({
                "task": "/can_ota",
                "canid": canid,
                "action": "verify",
                "md5": md5_hash
            }, timeout=15.0)
            
            if response and response.get('success'):
                print("  ✓ Verification successful!")
            else:
                print(f"  Verification response: {response}")
            
            # End command
            response = self.send_json({
                "task": "/can_ota",
                "canid": canid,
                "action": "end"
            }, timeout=10.0)
            
            print("\n" + "=" * 70)
            print("OTA UPLOAD COMPLETED!")
            print("=" * 70)
            print(f"Total time: {time.time() - start_time:.1f}s")
            print(f"Average speed: {speed:.1f} KB/s")
            
            self.close_serial()
            return True
            
        except KeyboardInterrupt:
            print("\n\nAborted by user!")
            return False
        except Exception as e:
            print(f"\nERROR: {e}")
            import traceback
            traceback.print_exc()
            return False
        finally:
            self.close_serial()


# ============================================================================
# MAIN
# ============================================================================

def main():
    """Command line interface"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="CAN OTA Binary Protocol Test Script",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python can_ota_test.py                                    # Use all defaults
  python can_ota_test.py -p /dev/ttyUSB0                    # Different port
  python can_ota_test.py -c 12 -f /path/to/firmware.bin     # Different CAN ID and firmware
  python can_ota_test.py -v                                 # Verbose debug output
  python can_ota_test.py -q                                 # Quiet mode (no debug)
        """
    )
    parser.add_argument("-p", "--port", default=DEFAULT_PORT,
                        help=f"Serial port (default: {DEFAULT_PORT})")
    parser.add_argument("-c", "--canid", type=int, default=DEFAULT_CAN_ID,
                        help=f"Target CAN ID (default: {DEFAULT_CAN_ID})")
    parser.add_argument("-f", "--firmware", default=DEFAULT_FIRMWARE,
                        help=f"Firmware binary file")
    parser.add_argument("--chunk-size", type=int, default=CHUNK_SIZE,
                        help=f"Chunk size in bytes (default: {CHUNK_SIZE})")
    parser.add_argument("--high-baud", type=int, default=HIGH_BAUDRATE,
                        help=f"High baudrate for data transfer (default: {HIGH_BAUDRATE})")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Verbose debug output (show all TX/RX)")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="Quiet mode (no TX/RX debug)")
    
    args = parser.parse_args()
    
    # Set debug level
    global DEBUG_VERBOSE
    if args.verbose:
        DEBUG_VERBOSE = True
    elif args.quiet:
        DEBUG_VERBOSE = False
    
    # Validate firmware exists
    if not Path(args.firmware).exists():
        print(f"ERROR: Firmware file not found: {args.firmware}")
        print("\nAvailable firmware files:")
        binaries_dir = Path(DEFAULT_FIRMWARE).parent
        if binaries_dir.exists():
            for f in binaries_dir.glob("*.bin"):
                print(f"  {f}")
        sys.exit(1)
    
    # Create uploader and run
    uploader = CANOTAUploader(
        port=args.port,
        normal_baud=NORMAL_BAUDRATE,
        high_baud=args.high_baud,
        chunk_size=args.chunk_size
    )
    
    success = uploader.upload_firmware(
        canid=args.canid,
        firmware_path=args.firmware
    )
    
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
