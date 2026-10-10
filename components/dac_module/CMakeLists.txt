# DAC module (C++) — needs the SPI driver.
idf_component_register(SRCS "src/DAC_Module.cpp" INCLUDE_DIRS "src" REQUIRES driver)
