# Scratch override: main.cpp moved to uc2_modules (link-order fix).
# This component now only provides include dirs + dep propagation.
idf_component_register(SRCS "stub.c"
                       INCLUDE_DIRS "." "${CMAKE_CURRENT_LIST_DIR}/../../../components/uc2_app"
                       REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update
                                esp_driver_ledc esp_driver_tsens
                                canopen_node uc2_od dac_module usb_host esp_usb_host_hid)
