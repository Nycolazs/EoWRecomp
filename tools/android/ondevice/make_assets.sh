#!/bin/bash
# tools/android/ondevice/make_assets.sh: what the APK carries for building the game code on the phone besides its
# programs (docs/BUILD-android.md), into build/ondevice/assets/ (android/app/build.gradle -PonDeviceBuild adds
# it to the APK's assets):
#   ondevice.zip   unpacked into files/ondevice by EowActivity.buildTools, once per app version:
#                    pyhome/                 the generator's Python: standard library + modules (build_python.sh)
#                    tools/                  the generator (tools/gen.py and its modules)
#                    cckit/                  the NDK pieces the compiler needs (build_cckit.sh)
#   licenses/      the license texts of everything the on-phone build adds to the APK (+ README.txt: versions and
#                  where their sources are)
# Nothing in here comes from the game: scripts, a Python runtime, headers and license texts.
set -euo pipefail
cd "$(dirname "$0")/../../.."
S=build/ondevice/stage O=build/ondevice/assets W=build/ondevice/obj/assets
[ -d $S/pyhome ] && [ -d $S/cckit ] && [ -f $S/jniLibs/arm64-v8a/libzig.so ] ||
    { echo "run build_python.sh, build_zig.sh and build_cckit.sh first (or build_all.sh)"; exit 1; }
rm -rf $O $W
mkdir -p $O/licenses $W/tools

cp -R $S/pyhome $S/cckit $W/
cp tools/gen.py tools/lifter.py tools/analyze.py tools/nso.py tools/passes.py tools/patches.py tools/simd_c.py $W/tools/
# (fixed dates and order: the same archive from the same inputs)
find $W -exec touch -h -t 202601010000 {} +
(cd $W && find . -type f | LC_ALL=C sort | sed 's|^\./||' | zip -q -X -9 -@ "$OLDPWD/$O/ondevice.zip")

# licenses: from the packages themselves, unicorn's and LLVM's from their upstream repositories at the pinned tags
fetch() {  # <url> <sha256> <out>
    local f=build/ondevice/licenses-dl/$(basename "$3")
    mkdir -p build/ondevice/licenses-dl
    [ -f "$f" ] || curl -fsSL --proto '=https' -o "$f" "$1"
    [ "$(shasum -a 256 "$f" | cut -d' ' -f1)" = "$2" ] || { echo "$1: sha256 mismatch"; exit 1; }
    cp "$f" "$3"
}
fetch https://raw.githubusercontent.com/unicorn-engine/unicorn/2.1.4/COPYING \
      6f04ae8364d0079a192b14635f4b1da294ce18724c034c39a6a41d1b09df6100 $O/licenses/unicorn-COPYING.txt
fetch https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-22.1.8/llvm/LICENSE.TXT \
      8d85c1057d742e597985c7d4e6320b015a9139385cff4cbae06ffc0ebe89afee $O/licenses/llvm-LICENSE.txt
cp $S/LICENSE.python.txt $O/licenses/python-LICENSE.txt
cp $S/LICENSE.capstone.txt $O/licenses/capstone-LICENSE.txt
cp $S/LICENSE.python-lz4.txt $O/licenses/python-lz4-LICENSE.txt
cp $S/LICENSE.zig.txt $O/licenses/zig-LICENSE.txt
cp build/ondevice/zig/lib/libc/musl/COPYRIGHT $O/licenses/musl-COPYRIGHT.txt
SDK=${ANDROID_HOME:-$(sed -n 's/^sdk.dir=//p' android/local.properties 2>/dev/null || true)}
[ -n "$SDK" ] && [ -d "$SDK/ndk" ] || SDK=$(sed -n 's/^sdk.dir=//p' "$(git rev-parse --git-common-dir)/../android/local.properties")
NDK=${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}
cp "$NDK/NOTICE" $O/licenses/android-ndk-NOTICE.txt
cp "$NDK/NOTICE.toolchain" $O/licenses/android-ndk-toolchain-NOTICE.txt
cat > $O/licenses/README.txt <<'EOF'
Building the game code on the phone: what this app carries for it, and their licenses
(EoWRecomp itself: MIT, see the project's LICENSE and README "License" for its other components)

CPython 3.14.8 (libpython3.14.so, libeowpython.so's interpreter, the standard library)
    PSF License 2.0 - python-LICENSE.txt (also lists libffi, zlib, bzip2, xz, zstd, mpdecimal used by its modules)
    source: https://www.python.org/downloads/release/python-3148/
Capstone 5.0.9 (libcapstone.so, its Python bindings)
    BSD 3-Clause - capstone-LICENSE.txt
    source: https://pypi.org/project/capstone/5.0.9/ , https://github.com/capstone-engine/capstone
Unicorn 2.1.4 (libunicorn.so; its Python bindings: BSD)
    GNU GPL version 2 - unicorn-COPYING.txt
    source (complete, as built): https://pypi.org/project/unicorn/2.1.4/#files (unicorn-2.1.4.tar.gz,
    sha256 00567a70e323f749b419cd86bee4f9115beab7ebba32194581c090cbb7c59cff), https://github.com/unicorn-engine/unicorn/tree/2.1.4
    built by tools/android/ondevice/build_python.sh in the EoWRecomp sources
python-lz4 4.4.5 with the LZ4 library (lz4 extension modules)
    BSD 3-Clause / BSD 2-Clause - python-lz4-LICENSE.txt
    source: https://pypi.org/project/lz4/4.4.5/
zig 0.17.0 (libzig.so: its clang and lld)
    MIT - zig-LICENSE.txt; LLVM (clang, lld): Apache 2.0 with LLVM Exceptions - llvm-LICENSE.txt;
    musl (linked into it): MIT - musl-COPYRIGHT.txt
    source: https://ziglang.org/download/0.17.0/ , https://github.com/llvm/llvm-project
Android NDK r27c pieces (bionic headers, crtbegin/crtend, libc/libm/libdl stubs, compiler-rt builtins)
    Apache 2.0 / BSD - android-ndk-NOTICE.txt, android-ndk-toolchain-NOTICE.txt
EOF
echo "$O: ondevice.zip $(du -h $O/ondevice.zip | cut -f1), licenses $(ls $O/licenses | wc -l | tr -d ' ') files"
