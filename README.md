# UC2-ESP Firmware for the openUC2 UC2e electronics

This repository provides the latest (`V2`) firmware that controls external hardware like Motors, LEDs, Lasers and other customized elements using an ESP32 and an adapter board. It is inspired by the [UC2-REST](https://github.com/openUC2/UC2-REST/tree/master/ESP32) firmware, but features a much more structured way of the code by dividing modules into seperated classes. A `ModuleController` ensures a proper initializiation of individual modules at runtime, which makes the entire code very modular and follows the overall UC2 principle. 

Similar to the legacy UC2-REST Firmware, the microcontroller can communicate using the wired serial and the wireless WiFi protocol. Both rely on a more-less similar `REST API` that uses endpoints to address an `act, get, set` command. For example, the information about the state of the ESP can be retrieved by issuing the code: 

```
{"task":"/state_get"}
```

A list of all commands that can be sent via HTTP requests and serial commands (e.g. by using the Arduino IDE-contained Serial monitor at 115200 BAUD) can be found in the [RestApi.md](./RestApi.md)-file. 

# Setting up the build environment

In order to build the code, you have to follow the following steps:

1. Install Visual Studio Code + the Extension called "Platform.io" => Restart Visual studio code to load PIO
2. Clone this repository including all the submodules: `git clone --recurse-submodules https://github.com/youseetoo/uc2-esp32`
3. Open the main folder in the Visual Studio Code 
4. Adjust the settings in the file `platformio.ini`-file (mostly the port)
5. Hit the `PlatformIO upload` button
6. open the PlatformIO serial monitor and check the ESP32's output
7. In case you have any problems: File an issue :-) 

Convert the firmware into a single file:
```
cd uc2-ESP/.pio/build/esp32dev
python -m esptool --chip esp32 merge_bin  -o merged-firmware.bin  --flash_mode dio  --flash_freq 40m  --flash_size 4MB  0x1000 bootloader.bin  0x8000 partitions.bin  0xe000 boot.bin  0x10000 firmware.bin
```

```
arduino-cli core update-index --config-file arduino-cli.yaml
arduino-cli core install esp32:esp32
arduino-cli board list

# compile
#arduino-cli compile --fqbn esp32:esp32:esp32-poe-iso .

mkdir build
arduino-cli compile --fqbn esp32:esp32:esp32:PSRAM=disabled,PartitionScheme=huge_app,CPUFreq=80 ESP32/main/main.ino  --output-dir ./build/ --libraries ./libraries/


esptool.py --chip esp32 --port /dev/cu.SLAB_USBtoUART --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x1000 /build/main.ino.bootloader.bin 0x8000 /build/main.ino.partitions.bin 0xe000 /build/boot_app0.bin 0x10000 /build/main.ino.bin 

# upload
arduino-cli upload -p /dev/cu.usbserial-1310 --fqbn esp32:esp32:esp32-poe-iso .




# screen the serial monitor
screen /dev/cu.usbserial-1310 115200
```

Upload via esptool:

```
pip install esptool
```

```
python -m esptool --chip esp32 --port /dev/cu.SLAB_USBtoUART --baud 921600  write_flash --flash_mode dio --flash_size detect 0x0 main.ino.bin
```
