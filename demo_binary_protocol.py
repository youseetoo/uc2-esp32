#!/usr/bin/env python3
"""
UC2 Binary Protocol Demo
Demonstrates the performance benefits and usage of the new binary protocol interface
"""

import sys
import struct
import time
import json

def simulate_message_sizes():
    """Demonstrate the size difference between JSON and binary messages"""
    
    print("=== UC2 Binary Protocol Demo ===\n")
    print("1. Message Size Comparison:")
    print("   (Simulated - showing typical overhead differences)\n")
    
    # Simulate common commands and their sizes
    commands = [
        {
            "name": "State Busy Check",
            "json": '{"task":"/b"}',
            "binary_size": 8,  # Just the header
            "description": "Quick status check"
        },
        {
            "name": "Get State Info", 
            "json": '{"task":"/state_get"}',
            "binary_size": 8,
            "description": "Get full device state"
        },
        {
            "name": "Motor Move Command",
            "json": '{"task":"/motor_act","steppers":[{"stepperid":0,"speed":1000,"position":500}],"isabs":1}',
            "binary_size": 8 + 16,  # Header + binary motor data
            "description": "Move motor to position"
        },
        {
            "name": "LED Array Control",
            "json": '{"task":"/ledarr_act","led":{"LEDArrMode":0,"led_array":[{"id":0,"r":255,"g":0,"b":0},{"id":1,"r":0,"g":255,"b":0}]}}',
            "binary_size": 8 + 32,  # Header + LED data
            "description": "Control LED array"
        }
    ]
    
    total_json_bytes = 0
    total_binary_bytes = 0
    
    for cmd in commands:
        json_size = len(cmd["json"])
        binary_size = cmd["binary_size"]
        savings = ((json_size - binary_size) / json_size) * 100
        
        print(f"   {cmd['name']}:")
        print(f"     JSON:   {json_size:3d} bytes - {cmd['json'][:50]}...")
        print(f"     Binary: {binary_size:3d} bytes - [binary header + payload]")
        print(f"     Savings: {savings:.1f}% reduction\n")
        
        total_json_bytes += json_size
        total_binary_bytes += binary_size
    
    overall_savings = ((total_json_bytes - total_binary_bytes) / total_json_bytes) * 100
    print(f"   Overall: {total_json_bytes} → {total_binary_bytes} bytes ({overall_savings:.1f}% reduction)\n")

def demonstrate_binary_format():
    """Show the binary message format"""
    
    print("2. Binary Message Format Example:")
    print("   (State busy check command)\n")
    
    # Create a binary message for state busy check
    magic_start = 0xAB
    version = 0x01
    command_id = 0x03  # CMD_STATE_BUSY
    flags = 0x00
    payload_length = 0x0000
    checksum = 0x0000
    
    binary_msg = struct.pack('<BBBBHH', magic_start, version, command_id, 
                           flags, payload_length, checksum)
    
    print(f"   Raw bytes: {' '.join(f'{b:02X}' for b in binary_msg)}")
    print("   Structure:")
    print(f"     Magic Start:     0x{magic_start:02X} (identifies binary protocol)")
    print(f"     Version:         0x{version:02X} (protocol version)")
    print(f"     Command ID:      0x{command_id:02X} (CMD_STATE_BUSY)")
    print(f"     Flags:           0x{flags:02X} (reserved)")
    print(f"     Payload Length:  0x{payload_length:04X} (no payload)")
    print(f"     Checksum:        0x{checksum:04X} (payload checksum)")
    print(f"     Total Size:      {len(binary_msg)} bytes\n")

def demonstrate_protocol_switching():
    """Show how protocol switching works"""
    
    print("3. Protocol Switching:")
    print("   The system supports three modes:\n")
    
    modes = [
        ("JSON_ONLY (0)", "Only accepts JSON messages", "Legacy compatibility"),
        ("BINARY_ONLY (1)", "Only accepts binary messages", "Maximum efficiency"),
        ("AUTO_DETECT (2)", "Auto-detects message format", "Default - best of both")
    ]
    
    for mode_name, description, use_case in modes:
        print(f"   {mode_name}:")
        print(f"     Description: {description}")
        print(f"     Use case: {use_case}\n")
    
    print("   Switch via JSON command:")
    print('   {"task":"/protocol_set", "mode": 2}\n')
    
    print("   Switch via binary command:")
    print("   Send CMD_PROTOCOL_SWITCH (0xF0) with mode byte\n")

def demonstrate_performance_benefits():
    """Show expected performance benefits"""
    
    print("4. Expected Performance Benefits:\n")
    
    benefits = [
        "Parsing Speed: Binary unpacking vs JSON string parsing",
        "Memory Usage: No string allocation for simple commands", 
        "Bandwidth: Especially beneficial for high-frequency status checks",
        "Latency: Reduced processing time per message",
        "Reliability: Built-in checksum validation"
    ]
    
    for benefit in benefits:
        print(f"   ✓ {benefit}")
    
    print("\n   Typical scenarios where binary protocol excels:")
    scenarios = [
        "Real-time motor control loops",
        "High-frequency sensor polling", 
        "LED animation sequences",
        "Status monitoring applications",
        "Automated scanning operations"
    ]
    
    for scenario in scenarios:
        print(f"     • {scenario}")

def demonstrate_backward_compatibility():
    """Show backward compatibility"""
    
    print("\n5. Backward Compatibility:\n")
    
    print("   ✓ Existing JSON interface remains fully functional")
    print("   ✓ No changes required to existing applications")
    print("   ✓ Binary protocol is opt-in via ENABLE_BINARY_PROTOCOL flag")
    print("   ✓ Runtime switching allows gradual migration")
    print("   ✓ Same controller endpoints work with both protocols")

def demonstrate_usage_examples():
    """Show usage examples"""
    
    print("\n6. Usage Examples:\n")
    
    print("   Python with UC2SerialBinary class:")
    print("   ```python")
    print("   from uc2_serial_binary import UC2SerialBinary")
    print("   import serial")
    print("   ")
    print("   ser = serial.Serial('/dev/ttyUSB0', 115200)")
    print("   uc2 = UC2SerialBinary(ser)")
    print("   ")
    print("   # Fast busy check using binary protocol")
    print("   busy = uc2.check_busy(use_binary=True)")
    print("   ")
    print("   # Get state using auto-detection")
    print("   state = uc2.get_state()")
    print("   ```\n")
    
    print("   Arduino/ESP32 firmware (automatic):")
    print("   ```cpp")
    print("   // JSON message - processed as before")
    print('   Serial.println(\'{"task":"/state_get"}\');')
    print("   ")
    print("   // Binary message - auto-detected and processed")
    print("   uint8_t binary_cmd[] = {0xAB, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};")
    print("   Serial.write(binary_cmd, sizeof(binary_cmd));")
    print("   ```")

def main():
    """Run the complete demo"""
    
    simulate_message_sizes()
    demonstrate_binary_format()
    demonstrate_protocol_switching()
    demonstrate_performance_benefits()
    demonstrate_backward_compatibility()
    demonstrate_usage_examples()
    
    print("\n=== Summary ===")
    print("The UC2 Binary Protocol provides:")
    print("• Significant size reduction for common commands")
    print("• Faster parsing and reduced memory usage")  
    print("• Auto-detection with seamless JSON fallback")
    print("• Runtime protocol switching capability")
    print("• Full backward compatibility")
    print("• Built-in message validation")
    
    print("\nReady for testing with actual UC2-ESP32 hardware!")
    print("Use the test_binary_client.py script to test with real devices.")

if __name__ == "__main__":
    main()