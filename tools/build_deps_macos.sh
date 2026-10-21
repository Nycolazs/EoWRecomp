#!/bin/bash
# make deps (macOS): the libraries and tools the shipped .app contains, built from pinned sources for the app's
# minimum macOS (MACOS_MIN, default 15.0) into build/deps. Homebrew's bottles target the macOS they were built on, so
# they can't ship. Plain `make` (dev builds) keeps using Homebrew.
#   SDL3; FFmpeg (LGPL only, no GPL parts: just what the game's movies need - WebM: VP9 video + Vorbis audio, plus
#   VP8 / Opus / H.264 / AAC in WebM or MP4 - and swscale); the Vulkan loader; shaderc; the Vulkan Memory Allocator
#   header; Python for the generator helper.
#   MoltenVK comes from Homebrew (it needs Xcode to build; its bottle already targets macOS 12).
# Needs: Xcode command line tools, cmake. Each step is skipped when its output exists (delete build/deps/<x> to redo).
set -euo pipefail
cd "$(dirname "$0")/.."
MIN=${MACOS_MIN:-15.0}
P=$PWD/build/deps
S=$P/src
mkdir -p "$S" "$P/bin" "$P/lib"
export MACOSX_DEPLOYMENT_TARGET=$MIN
JOBS=$(sysctl -n hw.ncpu)

fetch() {  # url sha256 file
    [ -f "$S/$3" ] || curl -fsSL -o "$S/$3.part" "$1" && { [ -f "$S/$3" ] || mv "$S/$3.part" "$S/$3"; }
    echo "$2  $S/$3" | shasum -a 256 -c --quiet
}
unpack() { rm -rf "$S/$2" && mkdir -p "$S/$2" && tar -xf "$S/$1" -C "$S/$2" --strip-components 1; }

fetch https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz \
      9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3 SDL3-3.4.18.tar.gz
fetch https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz \
      8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e ffmpeg-9.0.2.tar.xz
if [ ! -f "$P/lib/libSDL3.0.dylib" ]; then
    unpack SDL3-3.4.18.tar.gz sdl
    cmake -S "$S/sdl" -B "$S/sdl/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$P" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN" -DCMAKE_OSX_ARCHITECTURES=arm64 -DSDL_SHARED=ON -DSDL_STATIC=OFF \
          -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL_DOCS=OFF
    cmake --build "$S/sdl/build" -j"$JOBS" && cmake --install "$S/sdl/build"
fi

if [ ! -f "$P/lib/libavcodec.dylib" ]; then
    unpack ffmpeg-9.0.2.tar.xz ffmpeg
    (cd "$S/ffmpeg" && ./configure --prefix="$P" --cc=clang --arch=arm64 --enable-shared --disable-static \
        --extra-cflags="-mmacosx-version-min=$MIN" --extra-ldflags="-mmacosx-version-min=$MIN" \
        --disable-autodetect --disable-programs --disable-doc --disable-network --disable-avdevice --disable-avfilter \
        --disable-swresample --disable-everything \
        --enable-protocol=file --enable-demuxer=matroska,mov \
        --enable-decoder=vp9,vp8,vorbis,opus,h264,aac --enable-parser=vp9,vp8,vorbis,opus,h264,aac \
        --install-name-dir='@rpath' && make -j"$JOBS" && make install)
fi

# the Vulkan loader (Homebrew's targets the macOS it was built on); its headers' version from Homebrew's vulkan-headers
if [ ! -f "$P/lib/libvulkan.1.dylib" ]; then
    fetch https://github.com/KhronosGroup/Vulkan-Loader/archive/refs/tags/v1.4.363.tar.gz \
          cc520304a4d8de9a6a1699553d6090aef0c25f87ef6cbc16b7c84ebd99093264 Vulkan-Loader-1.4.363.tar.gz
    unpack Vulkan-Loader-1.4.363.tar.gz vulkan-loader
    cmake -S "$S/vulkan-loader" -B "$S/vulkan-loader/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX="$P" -DCMAKE_PREFIX_PATH=/opt/homebrew -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN" \
          -DCMAKE_OSX_ARCHITECTURES=arm64 -DBUILD_TESTS=OFF -DCMAKE_INSTALL_NAME_DIR='@rpath'
    cmake --build "$S/vulkan-loader/build" -j"$JOBS" && cmake --install "$S/vulkan-loader/build"
fi

# shaderc (the runtime's GLSL -> SPIR-V compiler, linked statically) with the glslang / SPIRV-Tools / SPIRV-Headers
# commits its DEPS file pins
if [ ! -f "$P/lib/libshaderc_combined.a" ]; then
    fetch https://github.com/google/shaderc/archive/refs/tags/v2026.4.tar.gz \
          f06ce5bcca94e5df7f34e115743597d0ad2e13c5fe9213c67dc8c76031241947 shaderc-2026.4.tar.gz
    fetch https://github.com/KhronosGroup/glslang/archive/e1b562a8bed273a02f30b59b66a5d499793cede5.tar.gz \
          907174a24713c6202c146f164bf81783f1fbc79c8cb821a30f18f159eb980312 glslang-e1b562a8.tar.gz
    fetch https://github.com/KhronosGroup/SPIRV-Tools/archive/ef96ed763b43b59b33b31b362f09a02b729fa1c9.tar.gz \
          82c62146083fd558735a3171cf97cfc47903ca7d368482e87f94bd44883c0f00 SPIRV-Tools-ef96ed76.tar.gz
    fetch https://github.com/KhronosGroup/SPIRV-Headers/archive/04fd3caa1e8267e4d95c806cad901181728e1006.tar.gz \
          392f4801409aad9c4f1b77745f179952fd6264e4c8bd0fc1bc45dfa6807bf6d0 SPIRV-Headers-04fd3caa.tar.gz
    unpack shaderc-2026.4.tar.gz shaderc
    T=$S/shaderc/third_party
    mkdir -p "$T/glslang" "$T/spirv-tools" "$T/spirv-headers"
    tar -xf "$S/glslang-e1b562a8.tar.gz" -C "$T/glslang" --strip-components 1
    tar -xf "$S/SPIRV-Tools-ef96ed76.tar.gz" -C "$T/spirv-tools" --strip-components 1
    tar -xf "$S/SPIRV-Headers-04fd3caa.tar.gz" -C "$T/spirv-headers" --strip-components 1
    cmake -S "$S/shaderc" -B "$S/shaderc/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$P" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN" -DCMAKE_OSX_ARCHITECTURES=arm64 -DSHADERC_SKIP_TESTS=ON \
          -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -DSPIRV_SKIP_EXECUTABLES=ON \
          -DENABLE_GLSLANG_BINARIES=OFF
    cmake --build "$S/shaderc/build" -j"$JOBS" --target shaderc_combined
    mkdir -p "$P/include/shaderc"
    cp "$S/shaderc/build/libshaderc/libshaderc_combined.a" "$P/lib/"
    cp "$S/shaderc/libshaderc/include/shaderc/"*.h "$S/shaderc/libshaderc/include/shaderc/"*.hpp "$P/include/shaderc/"
fi

# Python for the game code generator the app carries (make app's gen-helper): python-build-standalone, for macOS 11+
if [ ! -x "$P/python/bin/python3" ]; then
    fetch "https://github.com/astral-sh/python-build-standalone/releases/download/20261009/cpython-3.12.15%2B20261009-aarch64-apple-darwin-install_only.tar.gz" \
          83dcb04777c798bd95f3687acf9ae84b09860272ee6bdfb05be8bcf9447f75b8 cpython-3.12.15-macos.tar.gz
    mkdir -p "$P/python" && tar -xf "$S/cpython-3.12.15-macos.tar.gz" -C "$P/python" --strip-components 1
fi

# the Vulkan Memory Allocator header (the Vulkan backend; header-only)
if [ ! -f "$P/include/vma/vk_mem_alloc.h" ]; then
    fetch https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz \
          c4f6bbe6b5a45c2eb610ca9d231158e313086d5b1a40c9922cb42b597419b14e VulkanMemoryAllocator-3.3.0.tar.gz
    unpack VulkanMemoryAllocator-3.3.0.tar.gz vma
    mkdir -p "$P/include/vma" && cp "$S/vma/include/vk_mem_alloc.h" "$P/include/vma/"
fi

echo "build/deps ready (macOS $MIN):"
for f in "$P"/lib/*.dylib; do
    [ -L "$f" ] && continue
    printf '  %-28s minos %s\n' "$(basename "$f")" "$(otool -l "$f" | awk '/LC_BUILD_VERSION/ {b=1} b && /minos/ {print $2; exit}')"
done
