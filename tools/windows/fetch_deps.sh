#!/bin/bash
# Windows x86-64 build from Linux (tools/windows/build.sh): fetches what eowrecomp.exe links against into
# build/win/deps - MSYS2 UCRT64 packages (SDL3, shaderc with its glslang / SPIRV-Tools DLLs, the Vulkan headers and
# loader import library, Vulkan Memory Allocator, the GCC runtime DLLs those need) and BtbN's LGPL FFmpeg shared build.
# Versions are pinned; each file is fetched once (build/win/dl) and extracted again on every run.
set -euo pipefail
cd "$(dirname "$0")/../.."
DL=build/win/dl
OUT=build/win/deps
mkdir -p "$DL"
MSYS=https://repo.msys2.org/mingw/ucrt64
PKGS=(
    mingw-w64-ucrt-x86_64-sdl3-3.4.18-1-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-shaderc-2026.4-1-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-glslang-16.6.0-1-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-spirv-tools-3~1.4.363.0-1-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-vulkan-headers-1~1.4.363.0-1-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-vulkan-loader-1~1.4.363.0-1-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-vulkan-memory-allocator-3.4.0-1-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-libgcc-16.2.0-6-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-libstdc++-16.2.0-6-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-libwinpthread-14.0.0.r495.g63ebe6cdb-1-any.pkg.tar.zst
    mingw-w64-ucrt-x86_64-libiconv-1.19-1-any.pkg.tar.zst
)
FFZIP=ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip
FFURL=https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/$FFZIP
for p in "${PKGS[@]}"; do
    [ -f "$DL/$p" ] || curl -fsSL --proto '=https' -o "$DL/$p.part" "$MSYS/$(printf %s "$p" | sed "s/~/%7E/g; s/+/%2B/g")" && [ -f "$DL/$p" ] || mv "$DL/$p.part" "$DL/$p"
done
[ -f "$DL/$FFZIP" ] || { curl -fsSL --proto '=https' -o "$DL/$FFZIP.part" "$FFURL" && mv "$DL/$FFZIP.part" "$DL/$FFZIP"; }
rm -rf "$OUT" && mkdir -p "$OUT"
for p in "${PKGS[@]}"; do tar --zstd -xf "$DL/$p" -C "$OUT" ucrt64; done
unzip -q -o "$DL/$FFZIP" -d "$OUT"
mv "$OUT/${FFZIP%.zip}" "$OUT/ffmpeg"
echo "build/win/deps: $(ls "$OUT/ucrt64/bin" | grep -c '\.dll$') DLLs (ucrt64), FFmpeg $(ls "$OUT/ffmpeg/bin" | grep -c '\.dll$') DLLs"
