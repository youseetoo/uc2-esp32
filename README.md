# UC2-ESP Firmware for the openUC2 UC2e electronics

This repository provides the latest (`V2`) firmware that controls external hardware like Motors, LEDs, Lasers and other customized elements using an ESP32 and an adapter board. It is inspired by the [UC2-REST](https://github.com/openUC2/UC2-REST/tree/master/ESP32) firmware, but features a much more structured way of the code by dividing modules into seperated classes. A `ModuleController` ensures a proper initializiation of individual modules at runtime, which makes the entire code very modular and follows the overall UC2 principle.

Similar to the legacy UC2-REST Firmware, the microcontroller can communicate using the wired serial and the wireless WiFi protocol. Both rely on a more-less similar `REST API` that uses endpoints to address an `act, get, set` command. For example, the information about the state of the ESP can be retrieved by issuing the code:

```
{"task":"/state_get"}
```

A list of all commands that can be sent via HTTP requests and serial commands (e.g. by using the Arduino IDE-contained Serial monitor at 50000 BAUD) can be found in the [RestApi.md](./RestApi.md)-file.

# Setting up the build environment

Certainly! Below is a tutorial crafted for beginners who are interested in developing firmware for the UC2-ESP project using Visual Studio Code with PlatformIO. This project is specifically designed for the ESP32 platform, leveraging both Arduino and ESP-IDF frameworks as defined in its `platformio.ini` file.

### Prerequisites
Before diving into the development process, ensure you have the following:
1. **Visual Studio Code** installed on your computer.
2. **PlatformIO IDE** extension for Visual Studio Code.
3. **Git** installed for cloning the repository.

### Setting Up Your Development Environment

#### Step 1: Install Visual Studio Code and PlatformIO
1. Download and install **Visual Studio Code** (VS Code) from the [official website](https://code.visualstudio.com/).
2. Open VS Code and navigate to the Extensions view by clicking on the square icon on the sidebar or pressing `Ctrl+Shift+X`.
3. Search for "PlatformIO IDE" and click on the install button.
4. Restart VS Code to ensure the PlatformIO IDE extension is properly loaded.

#### Step 2: Clone the UC2-ESP Repository
1. Open a terminal or command prompt.
2. Clone the UC2-ESP repository by executing:
   ```
   git clone --recurse-submodules https://github.com/youseetoo/uc2-esp32
   ```
3. This command clones the repository and all its submodules, ensuring you have the complete codebase.

#### Step 3: Open the Project in VS Code
1. Open VS Code, go to the File menu, and select "Open Folder".
2. Navigate to the directory where you cloned the UC2-ESP repository and select the `uc2-esp32` folder.

#### Step 4: Configuration
1. Once the project is open in VS Code, locate the `platformio.ini` file in the root directory. This file contains the configuration for your project environment.
2. If your development board is connected, you can set the `upload_port` based on your system:
   - For Linux/Mac: `upload_port = /dev/cu.SLAB_USBtoUART`
   - For Windows: `upload_port = COM3` (adjust the COM port based on your system)
3. Examine the `PinConfig.h` file to select the appropriate board configuration by setting the desired `PinConfig` structure, e.g., `const UC2_3 pinConfig;`.

#### Step 5: Building and Uploading the Firmware
1. With the project open in VS Code, click on the PlatformIO icon on the sidebar.
2. Under the "Quick Access" section, locate your environment (e.g., `esp32dev`) and click on the "Upload" button. This action compiles the code and uploads it to your ESP32 board.
3. After uploading, the terminal will display messages about the process. A successful upload ends with "Leaving... Hard resetting via RTS pin...".

#### Step 6: Monitoring Output
1. To view the serial output from your ESP32, click on the "Monitor" button in the PlatformIO Quick Access section.
2. Ensure the `monitor_speed` in the `platformio.ini` file matches the baud rate set in your program (default is 500000 for this project).
3. If necessary, reset your ESP32 board to start the program from the beginning.

#### Step 7: Debugging and Troubleshooting
If you encounter issues during development or uploading:
- Double-check your `platformio.ini` configurations, especially the `upload_port` and board settings.
- Review the serial monitor output for any error messages.
- Ensure your ESP32 board is correctly connected to your computer.
- For specific problems, consider filing an issue in the [UC2-ESP GitHub repository](https://github.com/youseetoo/uc2-esp32) for support.

### Testing and Experimentation
To test various functionalities, refer to the `json_api_BD.txt` file in the `main` directory for a collection of JSON commands that can be sent to the ESP32.

This guide covers the basics to get started with firmware development for the UC2-ESP project using VS Code and PlatformIO. As you become more familiar with the project, explore the codebase to understand the implementation details and how you can contribute to its development.

In order to test several commands, you can find a useful list of `json`files in this file: [main/json_api_BD.txt](main/json_api_BD.txt)


# Flash the firmware using the Web-Tool

A new way to flash the firmware to the ESP32 is to use the open-source ESPHome Webtool. We have modified it such that the software in thi repo is compiled and uploaded to the website:

**[www.youseetoo.github.io]**(youseetoo.github.io)

There you can select the board you have and flash the code. If the driver is properly installed, you simply select the port and hit `start`. The firmware will automatically be downloaded and flashed through the browser.

<p align="center">
<img src="./IMAGES/webtool.png" width="550">
<br> This shows the gui to first select the board you have before the browser (chrome/edge) automatically flashes the firmware.
</p>

# Additional information

This is a fastly moving repo and the information may get outdated quickly. Please also check the relevant information in our [documentation](https://openuc2.github.io/docs/Electronics/)


# Information about the REST commands

Every component can be used by calling it in a REST-ful way. A not-complete document that describes the most important functions can be found [here](./RestApi.md)

The general structure for the serial would become:

````
{"task": "/state_get"}
````

to get the information about the MCU. A additional cheat-sheet can be found [here](main/json_api_BD.txt)

# Some Arduino-CLI/ESPTOOL/PlatformIO-related commands

This is a random collection of tasks/commands that are useful to compile/upload the code by hand.


Convert the firmware into a single file:

```
cd uc2-ESP/.pio/build/esp32dev
python -m esptool --chip esp32 merge_bin  -o merged-firmware.bin  --flash_mode dio  --flash_freq 40m  --flash_size 4MB  0x1000 bootloader.bin  0x8000 partitions.bin  0xe000 boot_app0.bin  0x10000 firmware.bin
```

Install the ESP32 core

```
arduino-cli core update-index --config-file arduino-cli.yaml
arduino-cli core install esp32:esp32
arduino-cli board list
```

Compile the firmware in a dedicated output folder

```
mkdir build
arduino-cli compile --fqbn esp32:esp32:esp32:PSRAM=disabled,PartitionScheme=huge_app,CPUFreq=80 ESP32/main/main.ino  --output-dir ./build/ --libraries ./libraries/
```

Flash the firmware to the ESP32 board

```
esptool.py --chip esp32 --port /dev/cu.SLAB_USBtoUART --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0x1000 /build/main.ino.bootloader.bin 0x8000 /build/main.ino.partitions.bin 0xe000 /build/boot_app0.bin 0x10000 /build/main.ino.bin
```

Upload the firmware using the Arduino-CLI

```
arduino-cli upload -p /dev/cu.usbserial-1310 --fqbn esp32:esp32:esp32-poe-iso .
```


Screen the serial monitor

```
screen /dev/cu.usbserial-1310 115200
```

Upload via esptool:

```
pip install esptool
python -m esptool --chip esp32 --port /dev/cu.SLAB_USBtoUART --baud 921600  write_flash --flash_mode dio --flash_size detect 0x0 main.ino.bin
```
