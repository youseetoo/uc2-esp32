# T1.9: uc2_serial — serial protocol + process (per-controller split).
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs
    "serial/SerialProcessIn.c"
    "serial/uc2_serial_out.c")
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    # uc2_bt: BtController.h is included under #ifdef BLUETOOTH (SerialProcess.cpp); the stub
    # links when UC2_BLUETOOTH is off, the real component when on.
    REQUIRES uc2_core uc2_hal uc2_canopen cjson nvs_flash driver app_update esp_driver_ledc
             esp_driver_tsens canopen_node uc2_od dac_module usb_host esp_usb_host_hid uc2_bt)
