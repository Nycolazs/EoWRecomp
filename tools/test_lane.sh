#!/bin/sh
# Lane id and mask translation test (tools/test_lane.c). Needs a C compiler only.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/test_lane
${CC:-cc} -O1 -w -o build/test_lane/test_lane tools/test_lane.c runtime/nvn/maxwell/translate.c
build/test_lane/test_lane
