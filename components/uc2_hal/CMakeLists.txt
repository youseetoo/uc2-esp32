# T1.3: thin C++ façades over direct ESP-IDF 6.1 calls (no Arduino, no shim).
# main/ is converted module-by-module to include these + call the IDF drivers
# directly. API surface = what main/ actually uses (grep-verified; see
# DOCS/PORT_PLAN_STEPS/refs/arduino-idf-mapping.md §2).
idf_component_register(
    SRCS "src/uc2_string.cpp" "src/uc2_esp.cpp" "src/uc2_serial.cpp"
         "src/uc2_gpio.cpp" "src/uc2_wire.cpp" "src/uc2_adc.cpp"
    INCLUDE_DIRS "include"
    REQUIRES log esp_timer esp_rom esp_system esp_hw_support heap freertos
             esp_driver_gpio esp_driver_ledc esp_driver_uart nvs_flash
             esp_driver_i2c esp_adc)
