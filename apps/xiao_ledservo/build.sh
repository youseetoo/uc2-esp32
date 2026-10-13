#!/bin/sh
# T2.1: canonical per-app build wrapper. Usage: cd apps/xiao_ledservo && ./build.sh
set -e
exec idf.py -D IDF_TARGET=esp32s3 -D PROJECT_DIR="$PWD" build
