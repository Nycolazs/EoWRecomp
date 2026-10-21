#!/bin/bash
# tools/test_moltenvk.sh [count] [jobs]: the shader translator's GLSL as MoltenVK would take it (macOS): fuzz
# programs (tools/test_translate.sh, glsl mode, the translations dumped) -> SPIR-V (glslangValidator, Vulkan 1.3) ->
# MSL (SPIRV-Cross, as MoltenVK does it, MSL 3.0) -> compiled by the Metal API (build/test_msl, tools/test_msl.m).
# Needs: brew install glslang spirv-cross shaderc spirv-tools. Failures are listed with Metal's message.
# (First run: 1015 of 3200 failed, popcount / fmin / clz ambiguous with Metal's built-ins: the GLSL helpers are mw_*.)
set -euo pipefail
cd "$(dirname "$0")/.."
COUNT=${1:-8000}
JOBS=${2:-8}
TT_DUMP_OK=$(( COUNT / JOBS )) REF=${REF:-93417ce} CC="${CC:-clang} -I/opt/homebrew/include -L/opt/homebrew/lib" \
    tools/test_translate.sh "$COUNT" "$JOBS" glsl > /dev/null 2>&1 || true
clang -O1 -fobjc-arc -framework Metal -framework Foundation tools/test_msl.m -o build/test_msl
W=build/test_moltenvk
mkdir -p "$W"
find "$W" -name '*.metal' -delete
n=0 conv=0
for f in build/test_translate/dump/glsl_ok_*.txt; do
    b=$W/$(basename "$f" .txt)
    if grep -q local_size_x "$f"; then st=comp; elif grep -q gl_Position "$f"; then st=vert; else st=frag; fi
    tail -n +3 "$f" > "$b.$st"
    n=$((n + 1))
    glslangValidator -V --target-env vulkan1.3 -S $st "$b.$st" -o "$b.spv" > /dev/null 2>&1 || { echo "glslang: $b.$st"; continue; }
    spirv-cross "$b.spv" --msl --msl-version 30000 --output "$b.metal" > "$b.cross" 2>&1 || { conv=$((conv + 1)); echo "spirv-cross: $b.spv"; }
done
echo "$n shaders, $conv SPIRV-Cross failures"
find "$W" -name '*.metal' -print0 | xargs -0 build/test_msl > "$W/result.txt" 2>&1 || true
echo "$(grep -c '^FAIL' "$W/result.txt" || true) of $(find "$W" -name '*.metal' | wc -l | tr -d ' ') failed in Metal"
# Expected leftovers of the method, not of MoltenVK: 1D textures (MoltenVK makes them 2D, MVK_CONFIG_TEXTURE_1D_AS_2D;
# the spirv-cross command line has no such option: get_width(lod), spvDepthCast on texture1d) and vertex attribute 31
# (Metal has 31; random programs pick any of Maxwell's 32)
grep -A3 '^FAIL' "$W/result.txt" | grep -o 'error: .*' | sed 's/[0-9]\{1,\}/N/g' | sort | uniq -c | sort -rn | head -20
