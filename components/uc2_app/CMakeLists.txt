# T1.4: app glue (refs/component-layout.md §3: uc2_app always registered).
# C++ until T4.6 (main.cpp is C++).
# T1.4: uc2_app still owns ALL gated sources, so REQUIRES is the union of
# what any of them (or main.cpp's headers) need. T1.5+ narrows this as the
# per-controller components take their sources out.
# T2.1: per-app PinConfig shadow — UC2_APP_MAIN_DIR (set by the app's top-level
# CMakeLists) is prepended by uc2_modinc.cmake; listed here for main.cpp itself.
# T4.1 batch (2026-10-19): main/src/dac/DacController.cpp removed — the PIO-only
# DacController was never used by the IDF tree (zero references in components/).
idf_component_register(SRCS "main.cpp"
                        "pinconfig_defaults.c"
                        INCLUDE_DIRS "." "$ENV{UC2_APP_MAIN_DIR}"
                                            "${CMAKE_CURRENT_LIST_DIR}/../uc2_periph/heat"
                         REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update
                                   esp_driver_ledc esp_driver_tsens esp_driver_i2s
                                  canopen_node uc2_od dac_module usb_host esp_usb_host_hid
                                  uc2_canopen uc2_led uc2_serial uc2_io uc2_periph tca9555)

# Bridge Kconfig options to the legacy C++ #ifdef macros (PIO compatibility).
# Each app's sdkconfig sets the CONFIG_UC2_* options; this defines the plain
# macros so the shared uc2_app/main.cpp source compiles unchanged.
if(CONFIG_UC2_PTZ_KEYBOARD_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE PTZ_KEYBOARD_CONTROLLER)
endif()
if(CONFIG_UC2_GPIO_CAN_SLAVE_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE GPIO_CAN_SLAVE_CONTROLLER)
endif()
if(CONFIG_UC2_JOYSTICK_USBHOST_PROVIDER)
    target_compile_definitions(__idf_uc2_app PRIVATE JOYSTICK_USBHOST_PROVIDER)
endif()
if(CONFIG_UC2_M5_DIAL_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE M5_DIAL_CONTROLLER)
endif()
if(CONFIG_UC2_CAN_CONTROLLER_CANOPEN)
    target_compile_definitions(__idf_uc2_app PRIVATE CAN_CONTROLLER_CANOPEN)
endif()
if(CONFIG_UC2_LED_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE LED_CONTROLLER)
endif()
if(CONFIG_UC2_LASER_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE LASER_CONTROLLER)
endif()
if(CONFIG_UC2_MOTOR_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE MOTOR_CONTROLLER)
endif()
if(CONFIG_UC2_HEAT_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE HEAT_CONTROLLER)
endif()
if(CONFIG_UC2_SCANNER_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE SCANNER_CONTROLLER)
endif()
if(CONFIG_UC2_I2C_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE I2C_CONTROLLER)
endif()
if(CONFIG_UC2_TCA_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE TCA_CONTROLLER)
endif()
if(CONFIG_UC2_ENCODER_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE ENCODER_CONTROLLER)
endif()
if(CONFIG_UC2_DIGITALIN_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE DIGITALIN_CONTROLLER)
endif()
if(CONFIG_UC2_DIGITALOUT_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE DIGITALOUT_CONTROLLER)
endif()
if(CONFIG_UC2_PID_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE PID_CONTROLLER)
endif()
if(CONFIG_UC2_BT_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE BT_CONTROLLER)
endif()
if(CONFIG_UC2_WIFI_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE WIFI_CONTROLLER)
endif()
if(CONFIG_UC2_THERMAL_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE THERMAL_CONTROLLER)
endif()
if(CONFIG_UC2_DAC_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE DAC_CONTROLLER)
endif()
if(CONFIG_UC2_HOME_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE HOME_CONTROLLER)
endif()
if(CONFIG_UC2_GALVO_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE GALVO_CONTROLLER)
endif()
if(CONFIG_UC2_FAN_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE FAN_CONTROLLER)
endif()
if(CONFIG_UC2_MESSAGE_CONTROLLER)
    target_compile_definitions(__idf_uc2_app PRIVATE MESSAGE_CONTROLLER)
endif()
if(CONFIG_UC2_STAGE_SCAN)
    target_compile_definitions(__idf_uc2_app PRIVATE STAGE_SCAN)
endif()

# T4.1: pinconfig.c re-initializes PINCONFIG_DEFAULTS_FIELDS overrides;
# GCC 15 (gnu23 + -Wextra + -Werror) promotes -Woverride-init to an error.
target_compile_options(__idf_uc2_app PRIVATE "$<$<COMPILE_LANGUAGE:C>:-Wno-override-init>")
