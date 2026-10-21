# T1.9: uc2_io — digital in/out, laser, encoder (per-controller split).
# Gating per component-layout.md §3.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs
    "encoder/InterruptController.c")
if(CONFIG_UC2_DIGITAL_IN_CONTROLLER)
    list(APPEND srcs "digitalin/DigitalInController.c")
endif()
if(CONFIG_UC2_DIGITAL_OUT_CONTROLLER)
    list(APPEND srcs "digitalout/DigitalOutController.cpp")
endif()
if(CONFIG_UC2_LASER_CONTROLLER)
    list(APPEND srcs "laser/LaserController.cpp")
endif()
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_hal uc2_serial cjson nvs_flash driver app_update esp_driver_ledc
             esp_driver_tsens canopen_node uc2_od dac_module usb_host esp_usb_host_hid tca9555)
