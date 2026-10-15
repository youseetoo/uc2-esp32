# T3.3: pscontroller — classic PS3/PS4 L2CAP controller (Arduino-era C++ leaf, D2).
# Links against the `bt` component's Bluedroid stack: its L2CA_*/BTM_*/GAP_*/profiles
# extern "C" decls (stack/*.h, shipped with this leaf) resolve at link time to the
# symbols compiled into `bt` (stack/l2cap/l2c_api.c, stack/btm/*, stack/gap/*, ...).
# Gated on UC2_PSXCONTROLLER (off by default; the BLE HID path is the active route on
# current boards). The leaf's Arduino.h usage is replaced by the uc2_hal shim
# (String -> uc2::String) in PSController.{h,cpp}.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
if(CONFIG_UC2_PSXCONTROLLER)
  idf_component_register(
      SRCS "PSController.cpp"
      INCLUDE_DIRS "." ${UC2_MODULE_INCLUDE_DIRS}
      REQUIRES uc2_hal bt)
else()
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pscontroller_stub.c
       "/* pscontroller: UC2_PSXCONTROLLER off (stub) */\n")
  idf_component_register(
      SRCS ${CMAKE_CURRENT_BINARY_DIR}/pscontroller_stub.c
      INCLUDE_DIRS "." ${UC2_MODULE_INCLUDE_DIRS}
      REQUIRES uc2_hal)
endif()
