# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)

idf_component_register(SRCS ${app_sources})
target_add_binary_data(${COMPONENT_TARGET} "certs/prvtkey.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/servercert.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/index.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/jquery-3.6.1.min.js" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/ota.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/script.js" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "data/styles.css" TEXT)