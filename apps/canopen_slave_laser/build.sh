#!/bin/sh
# T1.5: canonical per-app build wrapper. All later steps reuse this pattern.
# Usage: cd apps/canopen_slave_laser && ./build.sh
set -e
exec idf.py -D IDF_TARGET=esp32s3 -D PROJECT_DIR="$PWD" build
