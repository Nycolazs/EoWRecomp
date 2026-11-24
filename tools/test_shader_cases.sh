#!/bin/sh
# Directed translator cases (tools/test_shader_cases.c). CC defaults to clang; zig works too (CC="zig cc").
set -e
cd "$(dirname "$0")/.."
OUT=${OUT:-build/test_shader_cases}
mkdir -p "$OUT"
PRE=""
case "$(uname -s)" in MINGW*|MSYS*) PRE="-include malloc.h";; esac
${CC:-clang} -O1 -Wall $PRE tools/test_shader_cases.c runtime/nvn/maxwell/translate.c -o "$OUT/test_shader_cases" -lm
"$OUT/test_shader_cases"
