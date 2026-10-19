# T1.9: uc2_core — state, config, qid, message, buzzer (per-controller split).
# Kconfig home for all UC2_* feature symbols (Kconfig next to this file).
# Gating per refs/component-layout.md §3.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs
    "state/State.c"
    "config/NVSConfig.c"
    "config/ConfigController.c"
    "qid/QidRegistry.c"
    "buzzer/BuzzerController.cpp")
if(CONFIG_UC2_MESSAGE_CONTROLLER)
    list(APPEND srcs "message/MessageController.cpp")
endif()
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_hal cjson nvs_flash driver app_update esp_driver_ledc
             esp_driver_tsens canopen_node uc2_od dac_module usb_host esp_usb_host_hid)
