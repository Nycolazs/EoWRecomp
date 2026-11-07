#!/bin/bash
# tools/android/build_deps.sh: what the Android app builds against, from pinned sources into build/android/deps
# (arm64-v8a, API 30): an LGPL-only FFmpeg with just what the game's movies need (the configuration of
# tools/linux/build_deps.sh, static), shaderc (GLSL -> SPIR-V, one static library), and the headers: Vulkan, Vulkan
# Memory Allocator, volk (Vulkan loaded at run time: drivers older than 1.3 and custom drivers). SDL3 is unpacked into
# build/android/SDL3 and built by the app's CMake (android/app/CMakeLists.txt) with its Java sources.
# Each part is skipped when its output exists (delete build/android/deps to redo).
set -euo pipefail
cd "$(dirname "$0")/../.."
S=$PWD/build/android/src
P=$PWD/build/android/deps
W=$PWD/build/android/work
mkdir -p "$S" "$P/include" "$P/lib" "$W"
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc)
API=30

SDK=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$(sed -n 's/^sdk.dir=//p' android/local.properties 2>/dev/null || true)}}
SDK=${SDK:-/opt/homebrew/share/android-commandlinetools}
NDK=${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}
[ -d "$NDK" ] || { echo "Android NDK 27.2 not found ($NDK): sdkmanager 'ndk;27.2.12479018'"; exit 1; }
case "$(uname -s)" in Darwin) HOST=darwin-x86_64 ;; *) HOST=linux-x86_64 ;; esac
TC=$NDK/toolchains/llvm/prebuilt/$HOST
CMAKE_ANDROID=(-DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a
               -DANDROID_PLATFORM=android-$API -DANDROID_STL=c++_static)

fetch() {  # url sha256 file
    [ -f "$S/$3" ] || { curl -fsSL -o "$S/$3.part" "$1" && mv "$S/$3.part" "$S/$3"; }
    echo "$2  $S/$3" | shasum -a 256 -c --quiet
}
unpack() { rm -rf "$W/$2" && mkdir -p "$W/$2" && tar -xf "$S/$1" -C "$W/$2" --strip-components 1; }

fetch https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz \
      9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3 SDL3-3.4.18.tar.gz
fetch https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz \
      8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e ffmpeg-9.0.2.tar.xz
fetch https://github.com/google/shaderc/archive/refs/tags/v2026.3.tar.gz \
      ee493ccf1b3038b4ef2fe024664c5eb2dc4bcc1f6b05b33e3909de0e19c81024 shaderc-2026.3.tar.gz
fetch https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.350.1.tar.gz \
      6d1bb65e49520344cc0a48af3dc02e993781efff14c7ebdcb8ae9fa23ddf7e83 Vulkan-Headers-1.4.350.1.tar.gz
fetch https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz \
      c4f6bbe6b5a45c2eb610ca9d231158e313086d5b1a40c9922cb42b597419b14e VulkanMemoryAllocator-3.3.0.tar.gz
fetch https://github.com/zeux/volk/archive/refs/tags/vulkan-sdk-1.4.350.1.tar.gz \
      078a9411298e4e0f60f5f5398c890783427c25a414619294ca8e69587bbd5eae volk-1.4.350.1.tar.gz
# libadrenotools (BSD-2) + liblinkernsbypass: the system Vulkan loader opened onto a custom driver (Turnip, below)
fetch https://github.com/bylaws/libadrenotools/archive/8fae8ce254dfc1344527e05301e43f37dea2df80.tar.gz \
      ceffce971676d4cfdf348a082df06fc92a1dca6d95bea892a480d63f200961cb libadrenotools-8fae8ce.tar.gz
fetch https://github.com/bylaws/liblinkernsbypass/archive/aa3975893d83ef1bc84c321ec60c65fbf1287887.tar.gz \
      da1128c8aa771c4d24766b53a47822d0717baa5c536ca8491220402942b80638 liblinkernsbypass-aa39758.tar.gz

if [ ! -f build/android/SDL3/CMakeLists.txt ]; then
    rm -rf build/android/SDL3 && mkdir -p build/android/SDL3
    tar -xf "$S/SDL3-3.4.18.tar.gz" -C build/android/SDL3 --strip-components 1
fi
if [ ! -f "$P/include/vulkan/vulkan.h" ]; then
    unpack Vulkan-Headers-1.4.350.1.tar.gz vulkan-headers
    cp -R "$W/vulkan-headers/include/vulkan" "$W/vulkan-headers/include/vk_video" "$P/include/"
fi
if [ ! -f "$P/include/vma/vk_mem_alloc.h" ]; then
    unpack VulkanMemoryAllocator-3.3.0.tar.gz vma
    mkdir -p "$P/include/vma" && cp "$W/vma/include/vk_mem_alloc.h" "$P/include/vma/"
fi
if [ ! -f "$P/include/volk/volk.c" ]; then
    unpack volk-1.4.350.1.tar.gz volk
    mkdir -p "$P/include/volk" && cp "$W/volk/volk.h" "$W/volk/volk.c" "$W/volk/LICENSE.md" "$P/include/volk/"
fi
if [ ! -f build/android/adrenotools/CMakeLists.txt ]; then
    rm -rf build/android/adrenotools && mkdir -p build/android/adrenotools/lib/linkernsbypass
    tar -xf "$S/libadrenotools-8fae8ce.tar.gz" -C build/android/adrenotools --strip-components 1
    tar -xf "$S/liblinkernsbypass-aa39758.tar.gz" -C build/android/adrenotools/lib/linkernsbypass --strip-components 1
fi

# FFmpeg: static, LGPL only, just the movies' codecs (WebM: VP9 + Vorbis; the rest as on the other systems)
if [ ! -f "$P/lib/libavcodec.a" ]; then
    unpack ffmpeg-9.0.2.tar.xz ffmpeg
    (cd "$W/ffmpeg" && ./configure --prefix="$P" --enable-cross-compile --target-os=android --arch=aarch64 \
        --cc="$TC/bin/aarch64-linux-android$API-clang" --cxx="$TC/bin/aarch64-linux-android$API-clang++" \
        --nm="$TC/bin/llvm-nm" --ar="$TC/bin/llvm-ar" --ranlib="$TC/bin/llvm-ranlib" --strip="$TC/bin/llvm-strip" \
        --enable-static --disable-shared --enable-pic --extra-cflags="-O2" \
        --disable-autodetect --disable-programs --disable-doc --disable-network --disable-avdevice --disable-avfilter \
        --disable-swresample --disable-everything \
        --enable-protocol=file --enable-demuxer=matroska,mov \
        --enable-decoder=vp9,vp8,vorbis,opus,h264,aac --enable-parser=vp9,vp8,vorbis,opus,h264,aac \
        && make -j"$JOBS" && make install)
fi
grep -q 'GPL version 3\|--enable-gpl' "$P/lib/pkgconfig/libavcodec.pc" && { echo "deps: FFmpeg is not LGPL"; exit 1; }

# shaderc as one static library (glslang, SPIRV-Tools inside)
if [ ! -f "$P/lib/libshaderc_combined.a" ]; then
    unpack shaderc-2026.3.tar.gz shaderc
    (cd "$W/shaderc" && python3 utils/git-sync-deps)
    cmake -S "$W/shaderc" -B "$W/shaderc/build" -G Ninja "${CMAKE_ANDROID[@]}" -DCMAKE_BUILD_TYPE=Release \
        -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON -DSPIRV_SKIP_TESTS=ON \
        -DSPIRV_SKIP_EXECUTABLES=ON -DENABLE_GLSLANG_BINARIES=OFF -DSPIRV_TOOLS_BUILD_STATIC=ON -DBUILD_SHARED_LIBS=OFF
    cmake --build "$W/shaderc/build" --target shaderc_combined -j"$JOBS"
    mkdir -p "$P/include/shaderc"
    cp "$W/shaderc/libshaderc/include/shaderc/"*.h "$P/include/shaderc/"
    cp "$W/shaderc/build/libshaderc/libshaderc_combined.a" "$P/lib/"
    cp "$W/shaderc/LICENSE" "$P/shaderc-LICENSE.txt"
    rm -rf "$W/shaderc"  # (~1 GB of sources and objects)
fi
# Mesa Turnip (MIT), the Vulkan driver the app carries for Adreno phones whose own driver is old (runtime/nvn/vulkan/
# device.c loads it through libadrenotools; EOWRECOMP_DRIVER=system: the phone's). K11MCH1's 26.0.0 R8 build (Vulkan
# 1.4.335): on an AYN Thor (Adreno 740) Echoes of Wisdom draws with it, where 24.3.0 R9v2 (LAS Recomp's choice for the
# Adreno 650: newer ones hung that GPU) drew every pass black; it also has the shaderInt64 Qualcomm's 1.3.128 lacks.
TURNIP=Turnip_v26.0.0_R8.zip
if [ ! -f "$P/turnip/meta.json" ] || [ "$(cat "$P/turnip.zip.name" 2>/dev/null)" != "$TURNIP" ]; then
    fetch https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/$TURNIP \
          e634db0f929e2205e95511c769071817d0390180ec72c8e690bc76375e813715 $TURNIP
    rm -rf "$P/turnip" && mkdir -p "$P/turnip" && unzip -o -q "$S/$TURNIP" -d "$P/turnip" && echo "$TURNIP" > "$P/turnip.zip.name"
fi
echo "deps: $P"
