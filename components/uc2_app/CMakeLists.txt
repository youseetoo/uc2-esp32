# T1.4: app glue (refs/component-layout.md §3: uc2_app always registered).
# C++ until T4.6 (main.cpp is C++).
#
# main/ stays an include dir while the per-env module TUs still live in
# main/src (T1.5-T2.12 move them into components); drop it then.
# T1.4: uc2_app still owns ALL gated sources, so REQUIRES is the union of
# what any of them (or main.cpp's headers) need. T1.5+ narrows this as the
# per-controller components take their sources out.
idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "." "${CMAKE_CURRENT_LIST_DIR}/../../main"
                       REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update
                                esp_driver_ledc esp_driver_tsens
                                canopen_node uc2_od dac_module usb_host esp_usb_host_hid)
