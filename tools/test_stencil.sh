#!/bin/sh
# Stencil-aspect texture read test (tools/test_stencil.c). Needs a C compiler only.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/test_stencil
${CC:-cc} -O1 -w -o build/test_stencil/test_stencil tools/test_stencil.c runtime/nvn/maxwell/translate.c
build/test_stencil/test_stencil
