"""
UC2 Serial Interface Extensions for Binary Protocol
This module extends the UC2-REST serial interface to support binary protocol communication
"""

import struct
import time
from typing import Optional, Tuple, Union

# Binary protocol constants  
BINARY_PROTOCOL_MAGIC_START = 0xAB
BINARY_PROTOCOL_MAGIC_END = 0xCD
BINARY_PROTOCOL_VERSION = 0x01

# Command mappings
COMMAND_MAP = {
    "/state_get": 0x01,
    "/state_act": 0x02, 
    "/b": 0x03,
    "/motor_get": 0x10,
    "/motor_act": 0x11,
    "/ledarr_get": 0x20,
    "/ledarr_act": 0x21,
    "/laser_get": 0x30,
    "/laser_act": 0x31,
    "/digitalout_get": 0x40,
    "/digitalout_act": 0x41,
    "/digitalin_get": 0x42,
    "/digitalin_act": 0x43,
}

class UC2SerialBinary:
    """
    Extension to UC2 Serial class for binary protocol support
    
    This class can be used alongside or as a replacement for the existing
    UC2-REST mserial.py implementation to provide binary protocol support.
    """
    
    def __init__(self, serial_instance):
        """
        Initialize with an existing serial instance
        
        Args:
            serial_instance: The serial port instance (e.g., from pyserial)
        """
        self.serial = serial_instance
        self.protocol_mode = "auto"  # "json", "binary", "auto"
        
    def set_protocol_mode(self, mode: str):
        """Set the preferred protocol mode"""
        if mode in ["json", "binary", "auto"]:
            self.protocol_mode = mode
        else:
            raise ValueError("Mode must be 'json', 'binary', or 'auto'")
    
    def calculate_checksum(self, data: bytes) -> int:
        """Calculate simple checksum for binary protocol"""
        return sum(data) & 0xFFFF
    
    def send_binary_command(self, endpoint: str, payload: bytes = b'') -> bool:
        """
        Send a command using binary protocol
        
        Args:
            endpoint: JSON endpoint string (e.g., "/state_get")
            payload: Binary payload data
            
        Returns:
            bool: True if sent successfully
        """
        command_id = COMMAND_MAP.get(endpoint)
        if command_id is None:
            raise ValueError(f"Unknown endpoint: {endpoint}")
            
        payload_length = len(payload)
        checksum = self.calculate_checksum(payload) if payload else 0
        
        # Pack binary header
        header = struct.pack('<BBBBHH',
                           BINARY_PROTOCOL_MAGIC_START,
                           BINARY_PROTOCOL_VERSION, 
                           command_id,
                           0,  # flags
                           payload_length,
                           checksum)
        
        try:
            self.serial.write(header + payload)
            self.serial.flush()
            return True
        except Exception as e:
            print(f"Failed to send binary command: {e}")
            return False
    
    def read_binary_response(self, timeout: float = 5.0) -> Optional[Tuple[int, int, bytes]]:
        """
        Read binary response from device
        
        Args:
            timeout: Read timeout in seconds
            
        Returns:
            Tuple of (command_id, status, payload) or None if failed
        """
        original_timeout = self.serial.timeout
        self.serial.timeout = timeout
        
        try:
            # Read response header (9 bytes)
            header_data = self.serial.read(9)
            if len(header_data) != 9:
                return None
                
            # Unpack response
            magic_start, version, command_id, status, payload_length, checksum, magic_end = \
                struct.unpack('<BBBBHHB', header_data)
                
            # Validate magic bytes
            if magic_start != BINARY_PROTOCOL_MAGIC_START or magic_end != BINARY_PROTOCOL_MAGIC_END:
                return None
                
            # Read payload if present
            payload = b''
            if payload_length > 0:
                payload = self.serial.read(payload_length)
                if len(payload) != payload_length:
                    return None
                    
            return command_id, status, payload
            
        except Exception as e:
            print(f"Failed to read binary response: {e}")
            return None
        finally:
            self.serial.timeout = original_timeout
    
    def send_json_command(self, command_dict: dict) -> bool:
        """Send command using JSON protocol (existing functionality)"""
        try:
            import json
            command_str = json.dumps(command_dict)
            self.serial.write(command_str.encode('utf-8'))
            self.serial.flush()
            return True
        except Exception as e:
            print(f"Failed to send JSON command: {e}")
            return False
    
    def send_command(self, endpoint: str, payload: Union[dict, bytes] = None) -> Tuple[bool, Optional[dict]]:
        """
        Send command using appropriate protocol based on mode and data type
        
        Args:
            endpoint: Command endpoint (e.g., "/state_get")
            payload: Command payload - dict for JSON, bytes for binary
            
        Returns:
            Tuple of (success, response_data)
        """
        if self.protocol_mode == "binary" or (
            self.protocol_mode == "auto" and isinstance(payload, bytes)
        ):
            return self._send_binary_command_with_response(endpoint, payload or b'')
        else:
            return self._send_json_command_with_response(endpoint, payload or {})
    
    def _send_binary_command_with_response(self, endpoint: str, payload: bytes) -> Tuple[bool, Optional[dict]]:
        """Send binary command and get response"""
        if not self.send_binary_command(endpoint, payload):
            return False, None
            
        response = self.read_binary_response()
        if response is None:
            return False, None
            
        command_id, status, response_payload = response
        
        # Convert binary response to dict format for compatibility
        result = {
            "command_id": command_id,
            "status": status,
            "success": 1 if status == 0 else 0
        }
        
        if response_payload:
            try:
                # Try to decode as JSON string
                import json
                payload_str = response_payload.decode('utf-8')
                payload_json = json.loads(payload_str)
                result.update(payload_json)
            except:
                # If not JSON, include as raw bytes
                result["payload"] = response_payload
                
        return True, result
    
    def _send_json_command_with_response(self, endpoint: str, payload: dict) -> Tuple[bool, Optional[dict]]:
        """Send JSON command and get response (existing functionality)"""
        command = {"task": endpoint}
        command.update(payload)
        
        if not self.send_json_command(command):
            return False, None
            
        # Read JSON response (would need to implement based on existing UC2-REST format)
        # This is a simplified version
        try:
            response_line = ""
            while not response_line.startswith('++'):
                response_line = self.serial.readline().decode('utf-8').strip()
            
            if response_line.startswith('++'):
                json_line = self.serial.readline().decode('utf-8').strip()
                end_line = self.serial.readline().decode('utf-8').strip()
                
                if end_line == '--':
                    import json
                    return True, json.loads(json_line)
                    
        except Exception as e:
            print(f"Failed to read JSON response: {e}")
            
        return False, None
    
    # Convenience methods for common operations
    def get_state(self, use_binary: bool = None) -> Optional[dict]:
        """Get device state"""
        mode_backup = self.protocol_mode
        if use_binary is not None:
            self.protocol_mode = "binary" if use_binary else "json"
            
        try:
            success, response = self.send_command("/state_get")
            return response if success else None
        finally:
            self.protocol_mode = mode_backup
    
    def check_busy(self, use_binary: bool = True) -> Optional[bool]:
        """Check if device is busy (optimized for binary protocol)"""
        mode_backup = self.protocol_mode
        if use_binary:
            self.protocol_mode = "binary"
            
        try:
            success, response = self.send_command("/b")
            if success and response:
                return response.get("b", False)
            return None
        finally:
            self.protocol_mode = mode_backup
    
    def switch_protocol_mode(self, mode: int) -> bool:
        """Switch the device's protocol mode (0=JSON, 1=Binary, 2=Auto)"""
        success, response = self.send_command("/protocol_set", {"mode": mode})
        return success and response and response.get("success") == 1


# Example usage and performance comparison
def example_usage():
    """Example showing how to use the binary protocol extension"""
    
    import serial
    
    # Initialize serial connection (adjust port as needed)
    try:
        ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=5)
        time.sleep(2)  # Allow connection to stabilize
    except:
        print("Could not connect to serial port")
        return
    
    # Create binary protocol interface
    uc2_binary = UC2SerialBinary(ser)
    
    print("=== UC2 Binary Protocol Example ===")
    
    # Test protocol switching
    print("\n1. Testing protocol mode switching...")
    if uc2_binary.switch_protocol_mode(2):  # Auto-detect mode
        print("✓ Switched to auto-detect mode")
    
    # Test getting device state with both protocols
    print("\n2. Getting device state...")
    
    # Using JSON protocol
    start_time = time.time()
    state_json = uc2_binary.get_state(use_binary=False)
    json_time = time.time() - start_time
    print(f"JSON response time: {json_time:.3f}s")
    if state_json:
        print(f"JSON response: {state_json}")
    
    # Using binary protocol  
    start_time = time.time()
    state_binary = uc2_binary.get_state(use_binary=True)
    binary_time = time.time() - start_time
    print(f"Binary response time: {binary_time:.3f}s")
    if state_binary:
        print(f"Binary response: {state_binary}")
    
    # Performance comparison for busy checking
    print("\n3. Performance test - checking busy status...")
    
    num_checks = 10
    
    # JSON busy checks
    uc2_binary.set_protocol_mode("json")
    start_time = time.time()
    for _ in range(num_checks):
        busy = uc2_binary.check_busy(use_binary=False)
    json_total = time.time() - start_time
    
    # Binary busy checks
    uc2_binary.set_protocol_mode("binary") 
    start_time = time.time()
    for _ in range(num_checks):
        busy = uc2_binary.check_busy(use_binary=True)
    binary_total = time.time() - start_time
    
    print(f"JSON: {num_checks} checks in {json_total:.3f}s ({json_total/num_checks:.3f}s avg)")
    print(f"Binary: {num_checks} checks in {binary_total:.3f}s ({binary_total/num_checks:.3f}s avg)")
    
    if json_total > 0:
        improvement = ((json_total - binary_total) / json_total) * 100
        print(f"Binary protocol is {improvement:.1f}% faster")
    
    ser.close()
    print("\n=== Test completed ===")

if __name__ == "__main__":
    example_usage()