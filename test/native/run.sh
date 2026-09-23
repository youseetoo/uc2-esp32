#!/usr/bin/env sh
# Host-side tests for pure firmware headers (no PlatformIO env needed).
set -e
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/uc2_native_tests"
mkdir -p "$out"
c++ -std=c++11 -Wall -Wextra -o "$out/test_stage_scan_order" test_stage_scan_order.cpp
"$out/test_stage_scan_order"
