#!/usr/bin/env python3
"""
OTA Update via CAN Bus
======================

This script sends an OTA update command to an ESP32 slave device via CAN bus.
The slave will connect to the specified WiFi network and start an OTA server,
allowing firmware updates to be uploaded wirelessly.

Usage:
    python ota_update_via_can.py --port /dev/ttyUSB0 --canid 11 --ssid "MyWiFi" --password "MyPassword"
    
After the slave connects to WiFi, you can upload firmware using:
    platformio run -t upload --upload-port UC2-CAN-B.local
    
Or using Arduino IDE OTA upload feature.
"""

import argparse
import json
import serial
import time
import subprocess
import platform
from typing import Optional

def send_json_command(serial_port: serial.Serial, command: dict) -> bool:
    """
    Send a JSON command to the ESP32 master via serial port.
    
    Args:
        serial_port: Open serial port connection
        command: Dictionary containing the command
        
    Returns:
        True if command was sent successfully
    """
    try:
        json_str = json.dumps(command)
        print(f"Sending command: {json_str}")
        serial_port.write((json_str + "\n").encode('utf-8'))
        serial_port.flush()
        
        # Wait for response
        time.sleep(1)
        while serial_port.in_waiting:
            response = serial_port.readline().decode('utf-8', errors='ignore').strip()
            if response:
                print(f"Response: {response}")
        
        return True
    except Exception as e:
        print(f"Error sending command: {e}")
        return False

def start_ota_on_slave(port: str, canid: int, ssid: str, password: str, 
                       timeout: int = 300000, baudrate: int = 115200) -> bool:
    """
    Send OTA start command to a CAN slave device.
    
    Args:
        port: Serial port path (e.g., /dev/ttyUSB0, COM3)
        canid: CAN ID of the target slave device
        ssid: WiFi SSID to connect to
        password: WiFi password
        timeout: OTA server timeout in milliseconds (default 5 minutes)
        baudrate: Serial baudrate (default 115200)
        
    Returns:
        True if command was sent successfully
    """
    try:
        # Open serial connection to master
        print(f"Opening serial port {port} at {baudrate} baud...")
        ser = serial.Serial(port, baudrate, timeout=2)
        time.sleep(2)  # Wait for connection to establish
        
        # Clear any pending data
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Construct OTA command
        ota_command = {
            "task": "/can_act",
            "ota": {
                "canid": canid,
                "ssid": ssid,
                "password": password,
                "timeout": timeout
            }
        }
        
        # Send command
        success = send_json_command(ser, ota_command)
        
        if success:
            print(f"\nOTA command sent to CAN ID {canid}")
            print(f"The slave should now connect to WiFi: {ssid}")
            print(f"Hostname will be: UC2-CAN-{canid:X}.local")
            print(f"\nWait ~30 seconds for the device to connect to WiFi...")
            print(f"\nYou can now upload firmware using:")
            print(f"  platformio run -t upload --upload-port UC2-CAN-{canid:X}.local")
            print(f"  or")
            print(f"  arduino-cli upload -p UC2-CAN-{canid:X}.local --fqbn esp32:esp32:esp32")
        
        ser.close()
        return success
        
    except serial.SerialException as e:
        print(f"Serial port error: {e}")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False

def find_device_ip(hostname: str, timeout: int = 30) -> Optional[str]:
    """
    Try to resolve the mDNS hostname to an IP address.
    
    Args:
        hostname: mDNS hostname (e.g., UC2-CAN-B.local)
        timeout: Timeout in seconds
        
    Returns:
        IP address if found, None otherwise
    """
    print(f"\nTrying to resolve {hostname}...")
    
    start_time = time.time()
    while (time.time() - start_time) < timeout:
        try:
            # Try using ping to resolve hostname
            if platform.system().lower() == "windows":
                cmd = ["ping", "-n", "1", hostname]
            else:
                cmd = ["ping", "-c", "1", hostname]
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=5)
            
            # Parse output to find IP address
            if result.returncode == 0:
                # Simple parsing - could be improved
                for line in result.stdout.split('\n'):
                    if 'PING' in line or 'ping' in line:
                        # Extract IP from output
                        parts = line.split()
                        for part in parts:
                            if part.startswith('(') and part.endswith(')'):
                                ip = part.strip('()')
                                print(f"Found IP address: {ip}")
                                return ip
                            elif '.' in part and part.replace('.', '').isdigit():
                                print(f"Found IP address: {part}")
                                return part
        except subprocess.TimeoutExpired:
            pass
        except Exception as e:
            print(f"Error during hostname resolution: {e}")
        
        time.sleep(2)
        print(".", end="", flush=True)
    
    print("\nCould not resolve hostname")
    return None

def upload_firmware_ota(hostname: str, firmware_path: str) -> bool:
    """
    Upload firmware to the device using platformio or espota.py
    
    Args:
        hostname: Device hostname or IP address
        firmware_path: Path to the firmware binary file
        
    Returns:
        True if upload was successful
    """
    try:
        print(f"\nUploading firmware to {hostname}...")
        
        # Try using platformio
        cmd = ["platformio", "run", "-t", "upload", f"--upload-port={hostname}"]
        
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            print("Firmware upload successful!")
            return True
        else:
            print(f"Upload failed: {result.stderr}")
            return False
            
    except FileNotFoundError:
        print("PlatformIO not found. Please install it or use Arduino IDE for OTA upload.")
        return False
    except Exception as e:
        print(f"Error during firmware upload: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description="Send OTA update command to ESP32 slave via CAN bus",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Start OTA on CAN ID 11 (motor X)
  python ota_update_via_can.py --port /dev/ttyUSB0 --canid 11 --ssid "MyWiFi" --password "pass123"
  
  # With custom timeout (10 minutes)
  python ota_update_via_can.py --port COM3 --canid 20 --ssid "MyWiFi" --password "pass123" --timeout 600000
  
  # Find device after OTA start
  python ota_update_via_can.py --port /dev/ttyUSB0 --canid 11 --ssid "WiFi" --password "pass" --find-ip
        """
    )
    
    parser.add_argument("--port", "-p", required=True, 
                       help="Serial port (e.g., /dev/ttyUSB0, COM3)")
    parser.add_argument("--canid", "-c", type=int, required=True,
                       help="CAN ID of the target slave device (decimal)")
    parser.add_argument("--ssid", "-s", required=True,
                       help="WiFi SSID")
    parser.add_argument("--password", "-w", required=True,
                       help="WiFi password")
    parser.add_argument("--timeout", "-t", type=int, default=300000,
                       help="OTA server timeout in milliseconds (default: 300000 = 5 min)")
    parser.add_argument("--baudrate", "-b", type=int, default=115200,
                       help="Serial baudrate (default: 115200)")
    parser.add_argument("--find-ip", action="store_true",
                       help="Try to find device IP after sending OTA command")
    
    args = parser.parse_args()
    
    # Send OTA command
    success = start_ota_on_slave(
        port=args.port,
        canid=args.canid,
        ssid=args.ssid,
        password=args.password,
        timeout=args.timeout,
        baudrate=args.baudrate
    )
    
    if not success:
        print("\nFailed to send OTA command")
        return 1
    
    # Try to find device IP if requested
    if args.find_ip:
        hostname = f"UC2-CAN-{args.canid:X}.local"
        ip = find_device_ip(hostname, timeout=60)
        if ip:
            print(f"\nDevice is reachable at: {ip}")
            print(f"You can now upload firmware using:")
            print(f"  platformio run -t upload --upload-port={ip}")
        else:
            print(f"\nCould not find device at {hostname}")
            print("The device might still be connecting to WiFi...")
    
    return 0

if __name__ == "__main__":
    exit(main())
