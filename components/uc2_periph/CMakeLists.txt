# T1.9: uc2_periph — i2c bridge + slave dial, tmp102, fan (per-controller split).
# I2cBridge.c is always compiled; its body is internally gated by
# #ifdef I2C_BRIDGE_CONTROLLER (board combo, applied by the app).
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs
    "i2c/I2cBridge.c"
    # T4.1: i2c_master.cpp was dead code (never in any build: no app defines
    # I2C_MASTER and it was already absent from this list) — deleted.
    "i2c/i2c_slave_dial.cpp"
    # T3.3: TCA9535 I/O expander (UC2_2/3/4/wemos). C++ island around the
    # vendored TCA9555 class; exposes the pure-C tca_* API (tca_controller.h).
    # The body is internally #ifdef USE_TCA9535-gated, so compiling
    # unconditionally is safe (no-op on boards without the flag);
    # tca_set_external_pin() is the FastAccelStepper callback.
    "i2c/tca_controller.cpp")
if(CONFIG_UC2_TMP102_CONTROLLER OR CONFIG_UC2_FAN_CONTROLLER)
    list(APPEND srcs "tmp102/Tmp102Controller.c")
endif()
if(CONFIG_UC2_FAN_CONTROLLER)
    list(APPEND srcs "fan/FanController.c")
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
        "scanner/dac_mcp4822.c")
endif()
# T2.9: DS18b20 heat controller (heat sink thermal regulation for laser diode).
if(CONFIG_UC2_HEAT_CONTROLLER)
    list(APPEND srcs "heat/DS18b20Controller.cpp" "heat/HeatController.c")
endif()
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update esp_driver_ledc
              esp_driver_tsens esp_driver_i2s canopen_node uc2_od dac_module usb_host esp_usb_host_hid
             esp_driver_rmt esp_driver_spi onewire_ds18b20 tca9555)
