# T1.9: uc2_periph — i2c bridge + slave dial, tmp102, fan (per-controller split).
# I2cBridge.cpp is always compiled; its body is internally gated by
# #ifdef I2C_BRIDGE_CONTROLLER (board combo, applied by the app).
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs
    "i2c/I2cBridge.cpp"
    "i2c/i2c_slave_dial.cpp")
if(CONFIG_UC2_TMP102_CONTROLLER OR CONFIG_UC2_FAN_CONTROLLER)
    list(APPEND srcs "tmp102/Tmp102Controller.cpp")
endif()
if(CONFIG_UC2_FAN_CONTROLLER)
    list(APPEND srcs "fan/FanController.cpp")
endif()
# T2.4: thermal (NTC heat-sink monitoring) — ported to raw IDF from main/src/thermal.
if(CONFIG_UC2_THERMAL_CONTROLLER)
    list(APPEND srcs "thermal/ThermalController.cpp")
endif()
# T2.5: galvo scanner (CANopen galvo slave) — ported to raw IDF from main/src/scanner.
if(CONFIG_UC2_GALVO_CONTROLLER)
    list(APPEND srcs
        "scanner/GalvoController.cpp"
        "scanner/HighSpeedScannerCore.cpp"
        "scanner/DAC_MCP4822.cpp")
endif()
# T2.9: DS18b20 heat controller (heat sink thermal regulation for laser diode).
if(CONFIG_UC2_HEAT_CONTROLLER)
    list(APPEND srcs "heat/DS18b20Controller.cpp" "heat/HeatController.cpp")
endif()
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update esp_driver_ledc
             esp_driver_tsens canopen_node uc2_od dac_module usb_host esp_usb_host_hid
             esp_driver_rmt esp_driver_spi onewire_ds18b20)
