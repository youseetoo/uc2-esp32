# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

set(CONFIG "UC2_3" CACHE STRING "Board Config")
OPTION(ANALOG_IN_CONTROLLER "ANALOG_IN_CONTROLLER" OFF)
OPTION(ANALOG_JOYSTICK "AnalogJoystick" OFF)
OPTION(ANALOG_OUT_CONTROLLER "ANALOG_OUT_CONTROLLER" OFF)
OPTION(BLUETOOTH "BLUETOOTH" OFF)
OPTION(DAC_CONTROLLER "DAC" OFF)
OPTION(DIGITAL_IN_CONTROLLER "DIGITAL_IN_CONTROLLER" OFF)
OPTION(DIGITAL_OUT_CONTROLLER "DIGITAL_OUT_CONTROLLER" OFF)
OPTION(DIAL_CONTROLLER "DIAL_CONTROLLER" OFF)
OPTION(LINEAR_ENCODER_CONTROLLER "LINEAR_ENCODER_CONTROLLER" OFF)
OPTION(LASER_CONTROLLER "Laser" OFF)
OPTION(TMC_CONTROLLER "TMC" OFF)
OPTION(LED_CONTROLLER "LED" OFF)
OPTION(PID_CONTROLLER "PID_CONTROLLER" OFF)
OPTION(MOTOR_CONTROLLER "Motor" OFF)
OPTION(GALVO_CONTROLLER "Galvo" OFF)
OPTION(SCANNER_CONTROLLER "SCANNER_CONTROLLER" OFF)
OPTION(WIFI "WIFI" OFF)
OPTION(BTHID "BTHID" OFF)
OPTION(PSXCONTROLLER "psx controller" OFF)
OPTION(HOME_MOTOR "HomeMotor" OFF)
OPTION(STAGE_SCAN "StageScan" OFF)
OPTION(USE_TCA9535 "USE_TCA9535" OFF)
OPTION(HEAT_CONTROLLER "HEAT_CONTROLLER" OFF)
OPTION(I2C_MASTER "I2C_MASTER" OFF)
OPTION(I2C_SLAVE_SLAVE_DIAL "I2C_SLAVE" OFF)
OPTION(FAN_CONTROLLER "FAN_CONTROLLER" OFF)
OPTION(TMP102_CONTROLLER "TMP102_CONTROLLER" OFF)

# DS4 USB-OTG -> CANopen bridge (S3 only)
OPTION(JOYSTICK_USBHOST_PROVIDER "Joystick over USB Host HID" OFF)

# CAN Bus Options
OPTION(CAN_BUS_ENABLED "Enable CAN bus hardware" OFF)
OPTION(CAN_CONTROLLER_CANOPEN "CANopen stack via CANopenNode" OFF)

OPTION(ESPNOW_MASTER "ESPNOW_MASTER" OFF)
OPTION(ESPNOW_SLAVE_MOTOR "ESPNOW_SLAVE_MOTOR" OFF)
OPTION(ESPNOW_SLAVE_LASER "ESPNOW_SLAVE_LASER" OFF)
OPTION(ESPNOW_SLAVE_SLAVE_DIAL "ESPNOW_SLAVE" OFF)

# Copy the correct PinConfig.h file to the main directory depending on the selected configuration in platformio.ini
# file(COPY ${CMAKE_SOURCE_DIR}/main/config/${CONFIG}/PinConfig.h DESTINATION ${CMAKE_SOURCE_DIR}/main/)
# THIS IS DONE VIA PRESCRIPT

set(mainsrc ${CMAKE_SOURCE_DIR}/main/)

#FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/)
set(srcs    ${mainsrc}main.cpp
            ${mainsrc}JsonKeys.h
            ${mainsrc}cJSON/cJSON.h
            ${mainsrc}cJSON/cJSON_Utils.h
            ${mainsrc}src/serial/SerialProcess.cpp
            ${mainsrc}src/qid/QidRegistry.cpp
            ${mainsrc}src/state/State.cpp
            ${mainsrc}src/config/ConfigController.cpp
            ${mainsrc}src/config/NVSConfig.cpp
            ${mainsrc}src/encoder/InterruptController.cpp
         )

if(ANALOG_IN_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/analogin/AnalogInController.cpp)
endif()
if(ANALOG_JOYSTICK EQUAL 1)
    list(APPEND srcs ${mainsrc}src/analogin/AnalogJoystick.cpp)
endif()
if(BLUETOOTH EQUAL 1)
    list(APPEND srcs src/bt/BtController.cpp)
    if(BTHID)
        list(APPEND srcs src/bt/HidController.cpp)
        list(APPEND srcs src/bt/esp_hid_gap.c)
    endif()
endif()
if(DAC_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/dac/DacController.cpp)
endif()
if(DIGITAL_IN_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/digitalin/DigitalInController.cpp)
endif()
if(DIGITAL_OUT_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/digitalout/DigitalOutController.cpp)
endif()
if(GPIO_CAN_SLAVE_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/gpio_can/GpioCanSlave.cpp)
    # Generic raw-I2C passthrough — gated internally by I2C_BRIDGE_CONTROLLER
    # (PinConfig.h). Compiles to empty on gpio builds that don't enable it.
    list(APPEND srcs ${mainsrc}src/i2c/I2cBridge.cpp)
endif()
if(PTZ_KEYBOARD_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/ptz/PelcoParser.cpp)
    list(APPEND srcs ${mainsrc}src/ptz/PtzKeyboard.cpp)
    list(APPEND srcs ${mainsrc}src/ptz/PtzRouter.cpp)
endif()
if(DIAL_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/dial/DialController.cpp)
endif()
if(LINEAR_ENCODER_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/encoder/LinearEncoderController.cpp)
    list(APPEND srcs ${mainsrc}src/encoder/PCNTEncoderController.cpp)

    # New clean-slate axis/ feedback module (design v2, WP1–WP3). Coexists with
    # the legacy encoder path above until validated on hardware (WP10).
    list(APPEND srcs ${mainsrc}src/axis/EncoderBackend.cpp)
    list(APPEND srcs ${mainsrc}src/axis/AxisCalibration.cpp)
    list(APPEND srcs ${mainsrc}src/axis/AxisController.cpp)
endif()
if(LASER_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/laser/LaserController.cpp)
endif()
if(TMC_CONTROLLER EQUAL 1)
list(APPEND srcs ${mainsrc}src/tmc/TMCController.cpp)
endif()
if(LED_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/led/LedController.cpp)
    list(APPEND srcs ${mainsrc}src/led/Ws2812Rmt.cpp)
    list(APPEND srcs ${mainsrc}src/signal/SignalController.cpp)
endif()

# Buzzer / status indicator are tiny and pin-gated at runtime; always compile
list(APPEND srcs ${mainsrc}src/buzzer/BuzzerController.cpp)
if(MESSAGE_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/message/MessageController.cpp)
endif()
if(GALVO_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/scanner/GalvoController.cpp)
    list(APPEND srcs ${mainsrc}src/scanner/HighSpeedScannerCore.cpp)
    list(APPEND srcs ${mainsrc}src/scanner/DAC_MCP4822.cpp)
endif()
if(I2C_MASTER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/i2c/i2c_master.cpp)
endif()

if(I2C_SLAVE_DIAL EQUAL 1)
    list(APPEND srcs ${mainsrc}src/i2c/i2c_slave_dial.cpp)
endif()
# Routing table and device router — always compiled (all builds use them)
list(APPEND srcs ${mainsrc}src/canopen/DeviceRouter.cpp)
list(APPEND srcs ${mainsrc}src/canopen/RoutingTable.cpp)

# CANopen stack
if(CAN_CONTROLLER_CANOPEN EQUAL 1)
    list(APPEND srcs ${mainsrc}src/canopen/CANopenModule.cpp)
    list(APPEND srcs ${mainsrc}src/canopen/CanOpenOTA.cpp)
    list(APPEND srcs ${mainsrc}src/canopen/CanOpenOTAStreaming.cpp)
    list(APPEND srcs ${mainsrc}src/canopen/OtaBinaryReceive.cpp)
endif()

if(FAN_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/fan/FanController.cpp)
    list(APPEND srcs ${mainsrc}src/tmp102/Tmp102Controller.cpp)
endif()

if(MOTOR_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/motor/FocusMotor.cpp)
    list(APPEND srcs ${mainsrc}src/motor/MotorJsonParser.cpp)
    list(APPEND srcs ${mainsrc}src/motor/MotorGamePad.cpp)
    list(APPEND srcs ${mainsrc}src/motor/MotorEncoderConfig.cpp)

    if(USE_FASTACCEL EQUAL 1)
        list(APPEND srcs ${mainsrc}src/motor/FAccelStep.cpp)
    endif()
    if(USE_ACCELSTEP EQUAL 1)
        list(APPEND srcs ${mainsrc}src/motor/AccelStep.cpp)
    endif()
    if(STAGE_SCAN EQUAL 1)
        list(APPEND srcs ${mainsrc}src/motor/StageScan.cpp)
        list(APPEND srcs ${mainsrc}src/motor/FocusScan.cpp)
    endif()
    if(HOME_MOTOR EQUAL 1)
        list(APPEND srcs ${mainsrc}src/home/HomeMotor.cpp)
    endif()
endif()
if(PID_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/pid/PidController.cpp)
endif()
if(SCANNER_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/scanner/ScannerController.cpp)
endif()
if(WIFI EQUAL 1)
    FILE(GLOB_RECURSE wifi ${mainsrc}src/wifi/*.*)
    list(APPEND srcs ${wifi})
endif()
if(USE_TCA9535 EQUAL 1)
    list(APPEND srcs ${mainsrc}src/i2c/tca_controller.cpp)
endif()
if(HEAT_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/heat/DS18b20Controller.cpp)
    list(APPEND srcs ${mainsrc}src/heat/HeatController.cpp)
endif()
if(TMP102_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/tmp102/Tmp102Controller.cpp)
endif()
if(FAN_CONTROLLER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/fan/FanController.cpp)
endif()

if(JOYSTICK_USBHOST_PROVIDER EQUAL 1)
    list(APPEND srcs ${mainsrc}src/joystick/JoystickUsbHost.cpp)
    list(APPEND srcs ${mainsrc}src/joystick/JoystickRouter.cpp)
endif()

#idf_component_register(SRCS ${srcs})
idf_component_register(
    SRCS ${srcs}
    REQUIRES driver  # or driver_pcnt ?
)
