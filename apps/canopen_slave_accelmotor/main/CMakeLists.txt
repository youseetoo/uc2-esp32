# T2.6: app entry shim. The real app (main.cpp + all gated sources) lives in
# the uc2_app component; main.c here only calls uc2_app_start(). uc2_motor
# (the new component for T2.6) compiles the motor/home/axis/tmc/pid sources.
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES uc2_app uc2_motor)
