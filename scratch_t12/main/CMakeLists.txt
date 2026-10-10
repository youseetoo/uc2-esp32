idf_component_register(SRCS "main.c"
                       REQUIRES canopen_node uc2_od cjson esp_usb_host_hid dac_module driver)
