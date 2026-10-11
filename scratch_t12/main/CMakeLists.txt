idf_component_register(SRCS "main.cpp"
                       REQUIRES canopen_node uc2_od cjson usb_host esp_usb_host_hid dac_module driver)
