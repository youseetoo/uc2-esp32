#!/bin/bash
# T0.5 host-native test runner (parity with PlatformIO [env:test]).
#
# Builds ONE test binary from test/unity/test_*.cpp (incl. test_all.cpp, the
# runner that holds every RUN_TEST) + the production sources the suite
# exercises, using test/unity/stubs/ as the Arduino/ESP-IDF/FreeRTOS host
# layer.
#
# This is the bash twin of run_tests.ps1 (the MSVC variant); keep the two in
# sync (source lists, defines, the stale-object cleanup).
#
# Usage:
#   bash test/unity/run.sh          # build + run
#   bash test/unity/run.sh --build  # build only
#   bash test/unity/run.sh --run    # run existing binary
set -e
cd "$(dirname "$0")/.."   # repo root

BUILD_DIR=build/host_test
OBJ_DIR=$BUILD_DIR/obj
BIN=$BUILD_DIR/unity_tests
mkdir -p "$OBJ_DIR"
# Drop stale objects: the source set changes between builds (a .cpp can move
# between "$PROD_SRCS" and a test's #include), and leftover objects get pulled
# into the link by basename, causing duplicate symbols.
rm -f "$OBJ_DIR"/*.o

# ── Includes ─────────────────────────────────────────────────────────────────
INC="-Itest/unity/stubs -Itest/unity -Itest/unity/unity \
     -Imain -Imain/config/UC2_3_default -Imain/cJSON \
     -Ilib/ESP32_CanOpenNode -Ilib/ESP32_CanOpenNode/src -Ilib/uc2_od"

# UNITY_INCLUDE_DOUBLE: this Unity build EXCLUDES double-precision asserts by
# default (unity_internals.h:304), so TEST_ASSERT_EQUAL_DOUBLE fails with
# "Unity Double Precision Disabled". Must be defined for BOTH the test .cpp
# TUs (which call the macro) and unity.c (the macro implementation).
CXXFLAGS="-std=c++17 -g -O0 -Wall $INC -DUNIT_TEST -DCORE_DEBUG_LEVEL=5 -DUNITY_INCLUDE_DOUBLE"
CFLAGS="-std=c11 -g -O0 -Wall $INC -DUNITY_INCLUDE_DOUBLE"

# ── Sources ──────────────────────────────────────────────────────────────────
# test_all.cpp is picked up by the test_*.cpp glob (it is the runner).
TEST_SRCS=()
for f in test/unity/test_*.cpp; do TEST_SRCS+=("$f"); done
TEST_SRCS+=(test/unity/app_main_stubs.cpp)
for f in test/unity/mocks/*.cpp; do TEST_SRCS+=("$f"); done
TEST_SRCS+=(test/unity/stubs/host_impl.cpp)

# Production under test. The Unity tests use the "include the .cpp under test"
# pattern, so the controllers each test #includes (SerialProtocol, NVSConfig,
# CANopenModule, RoutingTable, AxisController, LedController, LaserController,
# MotorJsonParser, DeviceRouter) are ALREADY compiled into their test_*.o and
# must NOT be recompiled here (duplicate symbols). Only the transitive
# dependencies that no test #includes are compiled here.
PROD_SRCS=(
    main/src/serial/SerialProcess.cpp
    main/src/motor/StageScan.cpp
    main/src/axis/AxisCalibration.cpp
    main/src/axis/AxisWatchdog.cpp
    main/src/axis/EncoderBackend.cpp
    main/src/axis/EncoderMonitor.cpp
    main/src/axis/AxisNotify.cpp
    main/src/motor/FocusMotor.cpp
    main/src/qid/QidRegistry.cpp
    main/src/state/State.cpp
    main/src/buzzer/BuzzerController.cpp
    main/src/signal/SignalController.cpp
)

C_SRCS=(main/cJSON/cJSON.c test/unity/unity/unity.c)

build() {
    echo "▶ Compiling $((${#TEST_SRCS[@]} + ${#PROD_SRCS[@]})) C++ TUs..."
    local objs=()
    for f in "${TEST_SRCS[@]}" "${PROD_SRCS[@]}"; do
        local o=$OBJ_DIR/$(echo "$f" | tr '/' '_').o
        echo "  CXX $f"
        g++ $CXXFLAGS -c "$f" -o "$o"
        objs+=("$o")
    done
    echo "▶ Compiling ${#C_SRCS[@]} C TUs..."
    for f in "${C_SRCS[@]}"; do
        local o=$OBJ_DIR/$(echo "$f" | tr '/' '_').o
        echo "  CC  $f"
        gcc $CFLAGS -c "$f" -o "$o"
        objs+=("$o")
    done
    echo "▶ Linking $BIN"
    g++ -o "$BIN" "${objs[@]}"
}

run() {
    echo "▶ Running $BIN"
    "$BIN"
}

case "${1:-}" in
    --build) build ;;
    --run)   run ;;
    *)       build; run ;;
esac
