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
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update esp_driver_ledc
             esp_driver_tsens canopen_node uc2_od dac_module usb_host esp_usb_host_hid)
