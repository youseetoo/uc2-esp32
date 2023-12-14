# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/)
set(srcs    ${app_sources}main.cpp
            ${app_sources}JsonKeys.h
            ${app_sources}cJSON/cJSON.h
            ${app_sources}cJSON/cJSON_Utils.h
            ${CMAKE_SOURCE_DIR}/main/src/serial/SerialProcess.cpp
            ${CMAKE_SOURCE_DIR}/main/src/State/State.cpp
            ${CMAKE_SOURCE_DIR}/main/src/config/ConfigController.cpp
         )
#no idea why that not work, would avoid all that stuff below...
#set(configfolder CONFIG)
#include_directories(${app_sources}config/${configfolder}/)

FILE(GLOB_RECURSE motor ${CMAKE_SOURCE_DIR}/main/src/motor/*.*)
FILE(GLOB_RECURSE wifi ${CMAKE_SOURCE_DIR}/main/src/wifi/*.*)
FILE(GLOB_RECURSE bt ${CMAKE_SOURCE_DIR}/main/src/bt/*.*)

if(CONFIG STREQUAL "XYZ_MOTOR_JOYSTICK")
    include_directories(${app_sources}config/XYZ_MOTOR_JOYSTICK/)
    
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/analogin/AnalogJoystick.cpp
                     ${motor}
                     ${wifi}
                     ${bt})
endif()
if(CONFIG STREQUAL "XYZ_MOTOR_ENDSTOP_JOYSTICK")
    include_directories(${app_sources}config/XYZ_MOTOR_ENDSTOP_JOYSTICK/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/analogin/AnalogJoystick.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/home/HomeController.cpp
                     ${motor}
                     ${wifi}
                     ${bt})
endif()
if(CONFIG STREQUAL "X_MOTOR_64LED_PIN")
    include_directories(${app_sources}config/X_MOTOR_64LED_PIN/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/led/LedController.cpp
                     ${motor}
                    )
endif()
if(CONFIG STREQUAL "HoLiSheet")
    include_directories(${app_sources}config/HoLiSheet/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/led/LedController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/home/HomeController.cpp
                     ${motor}
                     ${wifi}
                     ${bt})
endif()
if(CONFIG STREQUAL "UC2_1")
    include_directories(${app_sources}config/UC2_1/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/led/LedController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/home/HomeController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/laser/LaserController.cpp
                     ${motor}
                     ${wifi}
                     ${bt})
endif()
if(CONFIG STREQUAL "UC2_Insert")
    include_directories(${app_sources}config/UC2_Insert/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/led/LedController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/home/HomeController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/laser/LaserController.cpp
                     ${motor}
                     ${wifi}
                     ${bt})
endif()
if(CONFIG STREQUAL "UC2_2")
    include_directories(${app_sources}config/UC2_2/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/led/LedController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/home/HomeController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/laser/LaserController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/digital/DigitalInController.cpp
                     ${motor}
                     ${wifi}
                     ${bt})
endif()
if(CONFIG STREQUAL "UC2e")
    include_directories(${app_sources}config/UC2e/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/led/LedController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/home/HomeController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/dac/DacController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/digital/DigitalInController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/analog/AnalogInController.cpp
                     ${motor}
                     )
endif()
if(CONFIG STREQUAL "UC2_WEMOS")
    include_directories(${app_sources}config/UC2_WEMOS/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/led/LedController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/home/HomeController.cpp
                     ${motor}
                     ${wifi}
                     ${bt})
endif()
if(CONFIG STREQUAL "UC2_OMNISCOPE")
    include_directories(${app_sources}config/UC2_OMNISCOPE/)
    list(APPEND srcs ${motor})
endif()
if(CONFIG STREQUAL "UC2_CassetteRecorder")
    include_directories(${app_sources}config/UC2_CassetteRecorder/)
    list(APPEND srcs ${motor})
endif()
if(CONFIG STREQUAL "UC2_XYZRotator")
    include_directories(${app_sources}config/UC2_XYZRotator/)
    list(APPEND srcs ${motor})
endif()
if(CONFIG STREQUAL "UC2_3")
    include_directories(${app_sources}config/UC2_3/)
    list(APPEND srcs ${CMAKE_SOURCE_DIR}/main/src/led/LedController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/home/HomeController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/laser/LaserController.cpp
                     ${CMAKE_SOURCE_DIR}/main/src/digital/DigitalInController.cpp
                     ${motor}
                     ${wifi}
                     ${bt})
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
