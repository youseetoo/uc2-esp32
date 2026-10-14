# esp32_encoder — linear encoder driver (PCNT). IDF 6.1: the legacy driver/pcnt.h
# is gone; PCNT is now esp_driver_pcnt (driver/pulse_cnt.h, handle-based).
# InterruptEncoder.cpp is dead code (self-referenced only); compile only
# ESP32Encoder.cpp so we don't pull esp_timer just for the unused interrupt path.
idf_component_register(SRCS "src/ESP32Encoder.cpp"
                       INCLUDE_DIRS "src"
                       REQUIRES esp_driver_pcnt esp_driver_gpio esp_common
                       PRIV_REQUIRES freertos)
