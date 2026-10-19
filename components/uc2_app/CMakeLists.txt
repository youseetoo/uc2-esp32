# T1.4: app glue (refs/component-layout.md §3: uc2_app always registered).
# C++ until T4.6 (main.cpp is C++).
#
# main/ stays an include dir while the per-env module TUs still live in
# main/src (T1.5-T2.12 move them into components); drop it then.
# T1.4: uc2_app still owns ALL gated sources, so REQUIRES is the union of
# what any of them (or main.cpp's headers) need. T1.5+ narrows this as the
# per-controller components take their sources out.
# T2.1: per-app PinConfig shadow — prepend UC2_APP_MAIN_DIR (set by the app's
# top-level CMakeLists) so the app-local PinConfig.h shadows the shared one.
# T3.3: DacController (JSON API over DAC_Module) still lives in main/src/dac and is
# called by main.cpp under #ifdef DAC_CONTROLLER; uc2_app owns it (see header note).
idf_component_register(SRCS "main.cpp"
                       "pinconfig_defaults.c"
                       "${CMAKE_CURRENT_LIST_DIR}/../../main/src/dac/DacController.cpp"
                       INCLUDE_DIRS "." "$ENV{UC2_APP_MAIN_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../main"
                                            "${CMAKE_CURRENT_LIST_DIR}/../uc2_periph/heat"
                         REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update
                                   esp_driver_ledc esp_driver_tsens esp_driver_i2s
                                  canopen_node uc2_od dac_module usb_host esp_usb_host_hid
                                  uc2_canopen uc2_led uc2_serial uc2_io uc2_periph tca9555)
