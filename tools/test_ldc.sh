#!/bin/sh
# LDC.64 address-alias test (tools/test_ldc.c). Needs a C compiler only.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/test_ldc
${CC:-cc} -O1 -w -o build/test_ldc/test_ldc tools/test_ldc.c runtime/nvn/maxwell/translate.c
build/test_ldc/test_ldc
