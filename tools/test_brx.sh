#!/bin/sh
# BRX jump-table labelling test (tools/test_brx.c). Needs a C compiler only.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/test_brx
${CC:-cc} -O1 -w -o build/test_brx/test_brx tools/test_brx.c runtime/nvn/maxwell/translate.c
build/test_brx/test_brx
