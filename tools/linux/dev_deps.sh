#!/bin/bash
# Development build on Ubuntu / Debian without root: the -dev packages fetched with `apt-get download` (no install)
# and unpacked into build/sysroot, a prefix for the Makefile's DEPS (build/deps-linux), zig (clang + lld) as
# build/bin/clang / clang++. The runtime libraries themselves (SDL3's, Vulkan loader, FFmpeg) come from the system,
# except SDL3 and shaderc, which are used from the sysroot (tools/run.sh sets LD_LIBRARY_PATH).
# Then: PATH=$PWD/build/bin:$PATH make -j$(nproc) DEPS=$PWD/build/deps-linux GENARCH=-mcpu=x86_64_v3
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT=$PWD
if [ ! -x build/zig-full/zig ]; then
    N=zig-x86_64-linux-0.17.0
    mkdir -p build/zig-dl
    [ -f build/zig-dl/$N.tar.xz ] || curl -fsSL --proto '=https' -o build/zig-dl/$N.tar.xz https://ziglang.org/download/0.17.0/$N.tar.xz
    echo "1cbe9df9f27e6b78d14ccbca43b6703a404ef79ef1c463de901d7f088d4e2026  build/zig-dl/$N.tar.xz" | sha256sum -c --quiet
    tar -xJf build/zig-dl/$N.tar.xz -C build/zig-dl && mv build/zig-dl/$N build/zig-full
fi
mkdir -p build/bin
printf '#!/bin/sh\nexec %s cc "$@"\n' "$ROOT/build/zig-full/zig" > build/bin/clang
printf '#!/bin/sh\nexec %s c++ "$@"\n' "$ROOT/build/zig-full/zig" > build/bin/clang++
chmod +x build/bin/clang build/bin/clang++
mkdir -p build/debs && (cd build/debs && apt-get download libsdl3-dev libsdl3-0 libvulkan-dev libshaderc-dev libshaderc1 \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev libvulkan-memory-allocator-dev)
R=$ROOT/build/sysroot
rm -rf "$R" && mkdir -p "$R"
for f in build/debs/*.deb; do dpkg-deb -x "$f" "$R"; done
D=$ROOT/build/deps-linux
rm -rf "$D" && mkdir -p "$D/include/vma" "$D/lib"
for x in "$R"/usr/include/* "$R"/usr/include/x86_64-linux-gnu/*; do
    [ "$(basename "$x")" = x86_64-linux-gnu ] || ln -sfn "$x" "$D/include/"
done
ln -sf "$R/usr/include/vk_mem_alloc.h" "$D/include/vma/"
for f in "$R"/usr/lib/x86_64-linux-gnu/*.so*; do  # (-dev symlinks pointing at system libraries: point them there)
    t=$(readlink -f "$f"); [ -e "$t" ] || t=/usr/lib/x86_64-linux-gnu/$(readlink "$f")
    ln -sf "$t" "$D/lib/$(basename "$f")"
done
echo "build/deps-linux ready"
