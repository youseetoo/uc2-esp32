# T3.3: uc2_bt — BT/PS4 controller layer (Bluedroid classic + BLE, HID host), gated on
# UC2_BLUETOOTH. Moved from main/src/bt.
#   - BtController.cpp: controller dispatch (classic PSController path + BLE HID host).
#   - esp_hid_gap.c:    BLE GAP/GATT glue for the HID host (PS4 over BLE), pure C.
#   - HidController.cpp: HID host input-event handling, further gated on UC2_BTHID.
# The classic PS3/PS4 L2CAP path links against components/pscontroller, whose L2CA_*/BTM_*
# calls resolve against the `bt` component's Bluedroid lib (symbols live in bt though not in
# public headers). When UC2_BLUETOOTH is off, register an empty stub so dependents
# (uc2_serial) still link.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
if(CONFIG_UC2_BLUETOOTH)
  set(uc2_bt_srcs "BtController.cpp" "esp_hid_gap.c")
  set(uc2_bt_requires uc2_core uc2_hal bt esp_hid nvs_flash esp_wifi esp_event driver)
  if(CONFIG_UC2_PSXCONTROLLER)
    list(APPEND uc2_bt_requires pscontroller)
  endif()
  if(CONFIG_UC2_BTHID)
    list(APPEND uc2_bt_srcs "HidController.cpp")
  endif()
  # IDF 6.1 exposes only bt/host/bluedroid/api/include/api as a *public* include of
  # `bt`; the classic-BT headers esp_bt.h (bt/include/<target>/include), esp_wifi.h
  # and esp_hid.h are not reliably reached via REQUIRES on this target, so add them
  # explicitly (REQUIRES still provides the link deps + the bluedroid api include).
  # NOTE: forward slashes are mandatory here — a Windows backslash path is parsed
  # by CMake as a character escape (e.g. M:\.espressif -> "\v"), breaking configure.
  set(_idf_path "$ENV{IDF_PATH}")
  string(REPLACE "\\" "/" _idf_path "${_idf_path}")
  set(uc2_bt_bt_incs
      "${_idf_path}/components/bt/include/esp32/include"
      "${_idf_path}/components/bt/host/bluedroid/api/include/api"
      "${_idf_path}/components/bt/common/api/include/api"
      "${_idf_path}/components/bt/common/include"
      "${_idf_path}/components/bt/common/btc/include"
      "${_idf_path}/components/esp_wifi/include"
      "${_idf_path}/components/esp_wifi/include/local"
      "${_idf_path}/components/esp_wifi/wifi_apps/include"
      "${_idf_path}/components/esp_hid/include"
      "${_idf_path}/components/esp_event/include"
      "${_idf_path}/components/esp_netif/include"
      "${_idf_path}/components/esp_phy/include"
      "${_idf_path}/components/esp_coex/include")
  idf_component_register(SRCS ${uc2_bt_srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS} ${uc2_bt_bt_incs}
    REQUIRES ${uc2_bt_requires})
  # Link-order fix (classic ESP32 + BT): IDF orders the `bt` component lib (libbt.a)
  # and the prebuilt controller lib (libbtdm_app.a) BEFORE the project component lib
  # (libuc2_bt.a), so the Bluedroid host symbols (esp_bt_gap_*, esp_ble_gap_*,
  # esp_hidh_*, esp_hid_usage_from_*, ...) that uc2_bt / pscontroller reference are
  # unresolved. Force them into a --start-group processed AFTER uc2_bt (same pattern
  # as the neopixel RMT fix) so the linker pulls them in regardless of order.
  # Also libesp_hid.a: the HID host API (esp_hidh_*, esp_hid_usage_str,
  # esp_hid_cod_*_str) lives in the `esp_hid` component, not `bt`; IDF orders it
  # before uc2_bt too, so group it here as well.
  target_link_libraries(__idf_uc2_bt PRIVATE
      "LINKER:--start-group"
      "LINKER:${CMAKE_BINARY_DIR}/esp-idf/bt/libbt.a"
      "LINKER:${_idf_path}/components/bt/controller/lib_esp32/esp32/libbtdm_app.a"
      "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_hid/libesp_hid.a"
      "LINKER:--end-group")
else()
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/uc2_bt_stub.c "/* uc2_bt: UC2_BLUETOOTH off (stub) */\n")
  idf_component_register(SRCS ${CMAKE_CURRENT_BINARY_DIR}/uc2_bt_stub.c
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_hal)
endif()
