#!/bin/sh
# SUST translation test (tools/test_sust.c). Needs a C compiler only.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/test_sust
${CC:-cc} -O1 -w -o build/test_sust/test_sust tools/test_sust.c runtime/nvn/maxwell/translate.c
build/test_sust/test_sust
