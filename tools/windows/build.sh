#!/bin/bash
# Windows x86-64 build from Linux: build/win/eowrecomp.exe (+ eowgame.dll when generated/ exists) and
# build/win/dist/EoWRecomp/ with the DLLs it needs. Needs build/zig-full (zig 0.17, see docs/STATUS.md) and
# tools/windows/fetch_deps.sh run once. Game code: the same generated/ as Linux (x86-64, portable SIMD).
set -euo pipefail
cd "$(dirname "$0")/../.."
D=build/win/deps
[ -d "$D/ucrt64" ] || tools/windows/fetch_deps.sh
P=$PWD/build/win/prefix  # one prefix for the Makefile's DEPS: MSYS2 UCRT64 + FFmpeg
rm -rf "$P" && mkdir -p "$P/include" "$P/lib"
for x in "$D"/ucrt64/include/* "$D"/ffmpeg/include/*; do ln -sfn "$PWD/$x" "$P/include/"; done
[ -e "$P/include/vma/vk_mem_alloc.h" ] || { echo "no vk_mem_alloc.h in the deps"; exit 1; }
# import libraries only (the DLLs ship next to the exe), named <name>.lib: zig's linker doesn't look for lib<name>.dll.a
for x in "$D"/ucrt64/lib/lib*.dll.a; do n=$(basename "$x" .dll.a); ln -sf "$PWD/$x" "$P/lib/${n#lib}.lib"; done
for x in "$D"/ffmpeg/lib/*.lib; do ln -sf "$PWD/$x" "$P/lib/"; done
make -j"$(nproc)" WINCROSS=1 B=build/win DEPS="$P" "$@"
OUT=build/win/dist/EoWRecomp
rm -rf "$OUT" && mkdir -p "$OUT"
cp build/win/eowrecomp.exe "$OUT/"
[ -f build/win/eowgame.dll ] && cp build/win/eowgame.dll build/win/eowgame.dll.id "$OUT/"
for dll in SDL3 libshaderc_shared libSPIRV-Tools libSPIRV-Tools-opt glslang SPIRV libglslang-default-resource-limits \
           libstdc++-6 libgcc_s_seh-1 libwinpthread-1 libiconv-2 libcharset-1; do
    [ -f "$D/ucrt64/bin/$dll.dll" ] && cp "$D/ucrt64/bin/$dll.dll" "$OUT/"
done
cp "$D"/ffmpeg/bin/*.dll "$OUT/"
cp third_party/fonts/Baloo2/Baloo2-SemiBold.ttf third_party/fonts/Baloo2/OFL.txt README.md LICENSE "$OUT/"
echo "$OUT: $(du -sh "$OUT" | cut -f1)"
