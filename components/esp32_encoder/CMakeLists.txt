# T5.1: esp32_encoder — C-ified from C++ class to flat C functions.
# Raw-IDF port using handle-based PCNT API (driver/pulse_cnt.h).
idf_component_register(SRCS "src/ESP32Encoder.c"
                       INCLUDE_DIRS "src"
                       REQUIRES driver esp_driver_pcnt esp_driver_gpio)