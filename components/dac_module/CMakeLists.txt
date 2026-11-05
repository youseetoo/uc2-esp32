# T5.1: DAC module (C) — needs the SPI driver.
idf_component_register(SRCS "src/dac_module.c" INCLUDE_DIRS "src" REQUIRES driver esp_driver_dac)