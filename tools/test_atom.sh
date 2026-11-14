#!/bin/sh
# ATOM translation test (tools/test_atom.c). Needs a C compiler only.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/test_atom
${CC:-cc} -O1 -w -o build/test_atom/test_atom tools/test_atom.c runtime/nvn/maxwell/translate.c
build/test_atom/test_atom
