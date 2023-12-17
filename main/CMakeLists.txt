# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

set(CONFIG "XYZ_MOTOR_JOYSTICK" CACHE STRING "Board Config")
OPTION(FOCUS_MOTOR "Motor" OFF)
OPTION(WIFI "WIFI" OFF)
OPTION(BLUETOOTH "BT" OFF)
OPTION(HOME_MOTOR "HomeMotor" OFF)
OPTION(ANALOG_JOYSTICK "AnalogJoystick" OFF)
OPTION(LED_CONTROLLER "LED" OFF)
OPTION(LASER_CONTROLLER "Laser" OFF)
OPTION(DAC_CONTROLLER "DAC" OFF)
OPTION(USE_TCA9535 "USE_TCA9535" OFF)


file(COPY ${CMAKE_SOURCE_DIR}/main/config/${CONFIG}/PinConfig.h DESTINATION ${CMAKE_SOURCE_DIR}/main/)

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/)
set(srcs    ${app_sources}main.cpp
            ${app_sources}JsonKeys.h
            ${app_sources}cJSON/cJSON.h
            ${app_sources}cJSON/cJSON_Utils.h
            ${app_sources}src/serial/SerialProcess.cpp
            ${app_sources}src/State/State.cpp
            ${app_sources}src/config/ConfigController.cpp
         )

FILE(GLOB_RECURSE motor ${app_sources}src/motor/*.*)
FILE(GLOB_RECURSE wifi ${app_sources}src/wifi/*.*)
FILE(GLOB_RECURSE bt ${app_sources}src/bt/*.*)

if(FOCUS_MOTOR EQUAL 1)
    list(APPEND srcs ${motor})
endif()
if(BLUETOOTH EQUAL 1)
    list(APPEND srcs ${bt})
endif()
if(WIFI EQUAL 1)
    list(APPEND srcs ${wifi})
endif()
if(HOME_MOTOR EQUAL 1)
    list(APPEND srcs ${app_sources}src/home/HomeMotor.cpp)
endif()
if(ANALOG_JOYSTICK EQUAL 1)
    list(APPEND srcs ${app_sources}src/analogin/AnalogJoystick.cpp)
endif()
if(LED_CONTROLLER EQUAL 1)
    list(APPEND srcs ${app_sources}src/led/LedController.cpp)
endif()
if(LASER_CONTROLLER EQUAL 1)
    list(APPEND srcs ${app_sources}src/laser/LaserController.cpp)
endif()
if(DIGITAL_IN_CONTROLLER EQUAL 1)
    list(APPEND srcs ${app_sources}src/digital/DigitalInController.cpp)
endif()
if(ANALOG_IN_CONTROLLER EQUAL 1)
    list(APPEND srcs ${app_sources}src/analog/AnalogInController.cpp)
endif()
if(DAC_CONTROLLER EQUAL 1)
    list(APPEND srcs ${app_sources}src/dac/DacController.cpp)
endif()

idf_component_register(SRCS ${srcs}

                            )

target_add_binary_data(${COMPONENT_TARGET} "certs/prvtkey.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/servercert.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/index.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/jquery.js" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/ota.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/script.js" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/styles.css" TEXT)
