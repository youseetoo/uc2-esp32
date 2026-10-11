#!/usr/bin/env sh
# Host-side tests for pure firmware headers (no PlatformIO env needed).
set -e
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/uc2_native_tests"
mkdir -p "$out"
c++ -std=c++11 -Wall -Wextra -o "$out/test_stage_scan_order" test_stage_scan_order.cpp
"$out/test_stage_scan_order"
# uc2::String (T1.3 uc2_hal) — pure C++, links the component source directly
c++ -std=c++11 -Wall -Wextra -I ../../components/uc2_hal/include \
    -o "$out/test_uc2_string" test_uc2_string.cpp ../../components/uc2_hal/src/uc2_string.cpp
"$out/test_uc2_string"
