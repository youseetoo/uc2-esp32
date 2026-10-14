idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "src"
    # uc2_hal: espidf_esp32.h routes Arduino GPIO through the shared HAL (D2).
    # esp_hal_gpio: the PCNT backend includes hal/gpio_hal.h for the iomux context.
    # esp_driver_*: the ESP-IDF v5 RMT + PCNT backends include their driver headers.
    REQUIRES
        esp_driver_gpio
        esp_driver_rmt
        esp_driver_pcnt
        esp_hal_gpio
        uc2_hal
        freertos
        esp_timer
        nvs_flash
        driver
        esp_hw_support
        esp_rom
        log
        soc
)
# Enable FAS driver-type selection (stepperConnectToPin(pin, DRIVER_MCPWM_PCNT/RMT)),
# which FAccelStep.cpp relies on to prefer MCPWM/PCNT with an RMT fallback. PUBLIC so
# consumers (uc2_motor) see the 2-arg overload + DRIVER_* constants too.
# (IDF names the component target __idf_<name>.)
target_compile_definitions(__idf_fastaccelstepper PUBLIC SUPPORT_SELECT_DRIVER_TYPE)
