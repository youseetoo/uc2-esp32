#!/usr/bin/env sh
# Host-side tests for pure firmware headers (no PlatformIO env needed).
set -e
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/uc2_native_tests"
mkdir -p "$out"
for t in test_stage_scan_order test_sync_filter test_strobe_timing test_strobe_sweep; do
    c++ -std=c++11 -Wall -Wextra -o "$out/$t" "$t.cpp"
    "$out/$t"
done
