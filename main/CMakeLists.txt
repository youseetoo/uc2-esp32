# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/)
FILE(GLOB_RECURSE app_sources2 ${CMAKE_SOURCE_DIR}/main/src/*.*)

idf_component_register(SRCS ${app_sources}main.cpp
                            ${app_sources2}
                            ${app_sources}Module.h
                            ${app_sources}Module.cpp
                            ${app_sources}ModuleController.h
                            ${app_sources}ModuleController.cpp
                            ${app_sources}PinConfig.h
                            ${app_sources}JsonKeys.h
                            ${app_sources}cJSON/cJSON.h
                            ${app_sources}cJSON/cJSON_Utils.h
                            )

target_add_binary_data(${COMPONENT_TARGET} "certs/prvtkey.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/servercert.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/index.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/jquery.js" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/ota.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/script.js" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/styles.css" TEXT)