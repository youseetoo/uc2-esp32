# T1.4: app glue (refs/component-layout.md §3: uc2_app always registered).
# C++ until T4.6 (main.cpp is C++).
# T1.4: uc2_app still owns ALL gated sources, so REQUIRES is the union of
# what any of them (or main.cpp's headers) need. T1.5+ narrows this as the
# per-controller components take their sources out.
# T2.1: per-app PinConfig shadow — UC2_APP_MAIN_DIR (set by the app's top-level
# CMakeLists) is prepended by uc2_modinc.cmake; listed here for main.cpp itself.
# T4.1 batch (2026-10-19): main/src/dac/DacController.cpp removed — the PIO-only
# DacController was never used by the IDF tree (zero references in components/).
idf_component_register(SRCS "main.cpp"
                       "pinconfig_defaults.c"
                       INCLUDE_DIRS "." "$ENV{UC2_APP_MAIN_DIR}"
                                            "${CMAKE_CURRENT_LIST_DIR}/../uc2_periph/heat"
                         REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update
                                   esp_driver_ledc esp_driver_tsens esp_driver_i2s
                                  canopen_node uc2_od dac_module usb_host esp_usb_host_hid
                                  uc2_canopen uc2_led uc2_serial uc2_io uc2_periph tca9555)
