#!/usr/bin/env python3
"""
Simple test utility for UC2 Binary Protocol
This script demonstrates how to send binary commands to the ESP32
"""

import serial
import struct
import json
import time
from typing import Optional, Tuple

# Binary protocol constants
BINARY_PROTOCOL_MAGIC_START = 0xAB
BINARY_PROTOCOL_MAGIC_END = 0xCD
BINARY_PROTOCOL_VERSION = 0x01

# Command IDs
CMD_STATE_GET = 0x01
CMD_STATE_ACT = 0x02
CMD_STATE_BUSY = 0x03
CMD_MOTOR_GET = 0x10
CMD_MOTOR_ACT = 0x11
CMD_LED_GET = 0x20
CMD_LED_ACT = 0x21
CMD_LASER_GET = 0x30
CMD_LASER_ACT = 0x31
CMD_PROTOCOL_SWITCH = 0xF0
CMD_PROTOCOL_INFO = 0xF1

class UC2BinaryClient:
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.ser: Optional[serial.Serial] = None
        
    def connect(self) -> bool:
        """Connect to the ESP32 device"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=5)
            time.sleep(2)  # Wait for connection to stabilize
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
            
    def disconnect(self):
        """Disconnect from the device"""
        if self.ser:
            self.ser.close()
            self.ser = None
            
    def calculate_checksum(self, data: bytes) -> int:
        """Calculate simple checksum"""
        return sum(data) & 0xFFFF
        
    def send_binary_command(self, command_id: int, payload: bytes = b'') -> bool:
        """Send a binary command to the ESP32"""
        if not self.ser:
            print("Not connected")
            return False
            
        # Create binary header
        payload_length = len(payload)
        checksum = self.calculate_checksum(payload) if payload else 0
        
        # Pack header: magic_start, version, command_id, flags, payload_length, checksum
        header = struct.pack('<BBBBHH', 
                           BINARY_PROTOCOL_MAGIC_START,
                           BINARY_PROTOCOL_VERSION,
                           command_id,
                           0,  # flags
                           payload_length,
                           checksum)
        
        # Send header + payload
        try:
            self.ser.write(header + payload)
            self.ser.flush()
            return True
        except Exception as e:
            print(f"Failed to send command: {e}")
            return False
            
    def read_binary_response(self) -> Optional[Tuple[int, int, bytes]]:
        """Read binary response from ESP32"""
        if not self.ser:
            return None
            
        try:
            # Read response header (9 bytes)
            header_data = self.ser.read(9)
            if len(header_data) != 9:
                print(f"Short read: got {len(header_data)} bytes")
                return None
                
            # Unpack response header
            magic_start, version, command_id, status, payload_length, checksum, magic_end = \
                struct.unpack('<BBBBHHB', header_data)
                
            if magic_start != BINARY_PROTOCOL_MAGIC_START or magic_end != BINARY_PROTOCOL_MAGIC_END:
                print(f"Invalid magic bytes: start={magic_start:02x}, end={magic_end:02x}")
                return None
                
            # Read payload if present
            payload = b''
            if payload_length > 0:
                payload = self.ser.read(payload_length)
                if len(payload) != payload_length:
                    print(f"Short payload read: expected {payload_length}, got {len(payload)}")
                    return None
                    
            return command_id, status, payload
            
        except Exception as e:
            print(f"Failed to read response: {e}")
            return None
            
    def send_json_command(self, json_data: dict) -> bool:
        """Send JSON command for comparison"""
        if not self.ser:
            return False
            
        try:
            json_str = json.dumps(json_data)
            self.ser.write(json_str.encode('utf-8'))
            self.ser.flush()
            return True
        except Exception as e:
            print(f"Failed to send JSON: {e}")
            return False
            
    def read_json_response(self) -> Optional[str]:
        """Read JSON response"""
        if not self.ser:
            return None
            
        try:
            # Wait for response markers
            line = self.ser.readline().decode('utf-8').strip()
            if line.startswith('++'):
                response_line = self.ser.readline().decode('utf-8').strip()
                end_marker = self.ser.readline().decode('utf-8').strip()
                if end_marker == '--':
                    return response_line
            return line
        except Exception as e:
            print(f"Failed to read JSON response: {e}")
            return None
    
    def test_protocol_info(self):
        """Test getting protocol information"""
        print("Testing protocol info...")
        
        # Send binary command
        if self.send_binary_command(CMD_PROTOCOL_INFO):
            response = self.read_binary_response()
            if response:
                cmd_id, status, payload = response
                print(f"Binary response - CMD: {cmd_id}, Status: {status}, Payload: {payload}")
                if payload:
                    try:
                        payload_str = payload.decode('utf-8')
                        print(f"Payload as string: {payload_str}")
                    except:
                        print(f"Payload as hex: {payload.hex()}")
            else:
                print("No binary response received")
                
    def test_state_busy(self):
        """Test state busy command"""
        print("Testing state busy...")
        
        if self.send_binary_command(CMD_STATE_BUSY):
            response = self.read_binary_response()
            if response:
                cmd_id, status, payload = response
                print(f"State busy - CMD: {cmd_id}, Status: {status}, Payload: {payload}")
            else:
                print("No response received")
                
    def compare_protocols(self):
        """Compare JSON vs Binary protocol performance"""
        print("Comparing JSON vs Binary protocols...")
        
        # Test JSON command
        print("Sending JSON command...")
        start_time = time.time()
        if self.send_json_command({"task": "/state_get"}):
            json_response = self.read_json_response()
            json_time = time.time() - start_time
            print(f"JSON response time: {json_time:.3f}s")
            print(f"JSON response: {json_response}")
        
        time.sleep(0.1)  # Small delay between tests
        
        # Test binary command
        print("Sending binary command...")
        start_time = time.time()
        if self.send_binary_command(CMD_STATE_GET):
            binary_response = self.read_binary_response()
            binary_time = time.time() - start_time
            print(f"Binary response time: {binary_time:.3f}s")
            if binary_response:
                cmd_id, status, payload = binary_response
                print(f"Binary response - CMD: {cmd_id}, Status: {status}")

def main():
    # This would need to be adjusted based on your ESP32's port
    # Linux: /dev/ttyUSB0, /dev/ttyACM0
    # Windows: COM3, COM4, etc.
    # macOS: /dev/cu.usbserial-*
    port = "/dev/ttyUSB0"
    
    client = UC2BinaryClient(port)
    
    if not client.connect():
        print("Failed to connect to device")
        return
        
    try:
        print("Connected to UC2 device")
        
        # Run tests
        client.test_protocol_info()
        time.sleep(1)
        
        client.test_state_busy()
        time.sleep(1)
        
        client.compare_protocols()
        
    finally:
        client.disconnect()
        print("Disconnected")

if __name__ == "__main__":
    main()