# T2.6: Vendored youseeetoo/AccelStepper fork (raw-IDF port, compiled by this component).
# Source copied from .pio/libdeps/seeed_xiao_esp32s3/AccelStepper (platformio.ini
# lib_deps: https://github.com/youseetoo/AccelStepper.git, used when USE_ACCELSTEP=1).
#
# Port: the non-Arduino include branch (WProgram.h/wiring.h) now pulls uc2_hal's
# Arduino-compat layer (pinMode/digitalWrite/digitalRead + HIGH/LOW/OUTPUT from
# uc2_hal/uc2_gpio.h, micros/millis/delay from uc2_hal/uc2_time.h).
idf_component_register(SRCS "src/AccelStepper.cpp" "src/MultiStepper.cpp"
                       INCLUDE_DIRS "src"
                       REQUIRES uc2_hal)
