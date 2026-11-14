#!/bin/sh
# VMNMX translation test (tools/test_vmnmx.c). Needs a C compiler only.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/test_vmnmx
${CC:-cc} -O1 -w -o build/test_vmnmx/test_vmnmx tools/test_vmnmx.c runtime/nvn/maxwell/translate.c
build/test_vmnmx/test_vmnmx
