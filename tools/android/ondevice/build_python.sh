#!/bin/bash
# tools/android/ondevice/build_python.sh: the generator's Python for building the game code ON the phone
# (docs/BUILD-android.md), made on this computer with the NDK. Into build/ondevice/stage/:
#   jniLibs/arm64-v8a/  libeowpython.so (the `python` program, tools/android/ondevice/pylauncher.c), libpython3.14.so,
#                       libcapstone.so (ARM64 only), libunicorn.so (aarch64 only)
#   pyhome/lib/python3.14/  CPython's standard library (trimmed) + lib-dynload/ + site-packages/ (capstone, unicorn,
#                           lz4 with its C extensions built here)
# Sources: CPython's official Android release (python.org, 3.14+), the capstone / unicorn / lz4 sdists from PyPI, all
# checked against their published sha256.
set -euo pipefail
cd "$(dirname "$0")/../../.."
ROOT=$PWD
SDK=${ANDROID_HOME:-$(sed -n 's/^sdk.dir=//p' android/local.properties 2>/dev/null || true)}
SDK=${SDK:-/opt/homebrew/share/android-commandlinetools}
[ -d "$SDK/ndk" ] || SDK=$(sed -n 's/^sdk.dir=//p' "$(git rev-parse --git-common-dir)/../android/local.properties")
NDK=${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}
case "$(uname -s)" in Darwin) HOST=darwin-x86_64; JOBS=$(sysctl -n hw.ncpu) ;; *) HOST=linux-x86_64; JOBS=$(nproc) ;; esac
TC=$NDK/toolchains/llvm/prebuilt/$HOST
API=30
CC="$TC/bin/aarch64-linux-android$API-clang"
STRIP=$TC/bin/llvm-strip
[ -x "$CC" ] || { echo "Android NDK 27.2 not found ($NDK)"; exit 1; }

PYVER=3.14.8 PYSHA=11324313957da3736f44e90257304d288864321a4dd376ed0cc35a54eacf9a33  # python.org release file api
CS=5.0.9 CSSHA=0429af292ddc604d3c9344696807e281d5e728db029f00e6a4ea9e3bff1aac9e      # PyPI sdists
UC=2.1.4 UCSHA=00567a70e323f749b419cd86bee4f9115beab7ebba32194581c090cbb7c59cff
LZ=4.4.5 LZSHA=5f0b9e53c1e82e88c10d7c180069363980136b9d7a8306c4dca4f760d60c39f0
B=build/ondevice
fetch() {  # <dir> <url> <sha256>: into its own folder, checked
    mkdir -p "$B/$1"
    local f=$B/$1/$(basename "$2")
    [ -f "$f" ] || curl -fL --proto '=https' -o "$f" "$2"
    local got; got=$(shasum -a 256 "$f" | cut -d' ' -f1)
    [ "$got" = "$3" ] || { echo "$f: sha256 $got, expected $3"; exit 1; }
}
fetch python-dl https://www.python.org/ftp/python/$PYVER/python-$PYVER-aarch64-linux-android.tar.gz $PYSHA
fetch capstone-dl https://files.pythonhosted.org/packages/39/01/6516910f546fbb996068207b9dd0229b14bc8dae223114d5e0e27d3cad11/capstone-$CS.tar.gz $CSSHA
fetch unicorn-dl https://files.pythonhosted.org/packages/b2/1b/b4248aa8422e86de690cf8e85cf8feae4c33405a097d1ebe71570bdaa6f5/unicorn-$UC.tar.gz $UCSHA
fetch lz4-dl https://files.pythonhosted.org/packages/57/51/f1b86d93029f418033dddf9b9f79c8d2641e7454080478ee2aab5123173e/lz4-$LZ.tar.gz $LZSHA

S=$B/src; mkdir -p $B/obj
mkdir -p $S
[ -d $B/python/prefix ] || { mkdir -p $B/python; tar -xzf $B/python-dl/python-$PYVER-aarch64-linux-android.tar.gz -C $B/python; }
[ -d $S/capstone-$CS ] || tar -xzf $B/capstone-dl/capstone-$CS.tar.gz -C $S
[ -d $S/unicorn-$UC ] || tar -xzf $B/unicorn-dl/unicorn-$UC.tar.gz -C $S
[ -d $S/lz4-$LZ ] || tar -xzf $B/lz4-dl/lz4-$LZ.tar.gz -C $S
PY=$B/python/prefix
PYINC=$PY/include/python3.14

ANDROID_CMAKE=(-G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a
               -DANDROID_PLATFORM=android-$API -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
               "-DCMAKE_SHARED_LINKER_FLAGS=-Wl,-z,max-page-size=16384 -Wl,--gc-sections"
               -DCMAKE_C_FLAGS=-ffunction-sections\ -fdata-sections)
echo "== libcapstone.so (ARM64 only)"
cmake -S $S/capstone-$CS/src -B $B/obj/capstone "${ANDROID_CMAKE[@]}" -DCAPSTONE_ARCHITECTURE_DEFAULT=OFF \
      -DCAPSTONE_ARM64_SUPPORT=ON -DCAPSTONE_BUILD_TESTS=OFF -DCAPSTONE_BUILD_CSTOOL=OFF -DBUILD_STATIC_LIBS=OFF \
      -DCAPSTONE_INSTALL=OFF > $B/obj/capstone.log
cmake --build $B/obj/capstone -j$JOBS >> $B/obj/capstone.log
echo "== libunicorn.so (aarch64 only)"
PKG_CONFIG=/usr/bin/false cmake -S $S/unicorn-$UC/src -B $B/obj/unicorn "${ANDROID_CMAKE[@]}" -DUNICORN_ARCH=aarch64 -DUNICORN_BUILD_TESTS=OFF \
      -DUNICORN_INSTALL=OFF -DUNICORN_LEGACY_STATIC_ARCHIVE=OFF > $B/obj/unicorn.log
PKG_CONFIG=/usr/bin/false cmake --build $B/obj/unicorn -j$JOBS >> $B/obj/unicorn.log

O=$B/stage
L=$O/jniLibs/arm64-v8a
H=$O/pyhome/lib/python3.14
rm -rf $O/pyhome $L/libeowpython.so $L/libpython3.14.so $L/libcapstone.so $L/libunicorn.so  # (its own: not libzig.so, cckit/)
mkdir -p $L $H/site-packages
cp $(find $B/obj/capstone -name 'libcapstone.so*' -type f | head -1) $L/libcapstone.so
cp $(find $B/obj/unicorn -name 'libunicorn.so*' -type f | head -1) $L/libunicorn.so
cp $PY/lib/libpython3.14.so $L/

echo "== libeowpython.so (the python program)"
"$CC" -O2 -fPIE -pie -I$PYINC tools/android/ondevice/pylauncher.c -o $L/libeowpython.so -L$L -lpython3.14 \
      -Wl,-z,max-page-size=16384 '-Wl,-rpath,$ORIGIN'  # (libpython beside it: no LD_LIBRARY_PATH in the app)

echo "== lz4 C extensions"
LZS=$S/lz4-$LZ
EXT=cpython-314-aarch64-linux-android.so
ext() {  # <module path> <sources...>
    local out=$H/site-packages/$1.$EXT; shift
    mkdir -p "$(dirname "$out")"
    "$CC" -O3 -shared -fPIC -I$PYINC -I$LZS/lz4libs "$@" -o "$out" -L$L -lpython3.14 -Wl,-z,max-page-size=16384
}
ext lz4/_version $LZS/lz4/_version.c $LZS/lz4libs/lz4.c
ext lz4/block/_block $LZS/lz4/block/_block.c $LZS/lz4libs/lz4.c $LZS/lz4libs/lz4hc.c
ext lz4/frame/_frame $LZS/lz4/frame/_frame.c $LZS/lz4libs/lz4.c $LZS/lz4libs/lz4hc.c $LZS/lz4libs/lz4frame.c \
    $LZS/lz4libs/xxhash.c
ext lz4/stream/_stream $LZS/lz4/stream/_stream.c $LZS/lz4libs/lz4.c $LZS/lz4libs/lz4hc.c
(cd $LZS && find lz4 -name '*.py' -exec sh -c 'mkdir -p "$1/$(dirname "$2")" && cp "$2" "$1/$2"' _ "$ROOT/$H/site-packages" {} \;)
# the bindings, pure Python: they find the libraries above by name (ctypes; LIBCAPSTONE_PATH / LIBUNICORN_PATH = the
# app's native library folder)
cp -R $S/capstone-$CS/capstone $H/site-packages/
cp -R $S/unicorn-$UC/unicorn $H/site-packages/
cp $S/capstone-$CS/LICENSE.TXT $O/LICENSE.capstone.txt
cp $LZS/LICENSE $O/LICENSE.python-lz4.txt
cp $S/unicorn-$UC/src/COPYING $O/LICENSE.unicorn.txt 2>/dev/null || true

echo "== standard library (trimmed)"
# what the generator doesn't use: tests, GUI, package installers, docs tools, the 2to3 era, sqlite, ssl, ...
(cd $PY/lib/python3.14 && tar -cf - --exclude test --exclude tests --exclude idlelib --exclude tkinter \
    --exclude turtledemo --exclude ensurepip --exclude venv --exclude pydoc_data --exclude lib2to3 --exclude sqlite3 \
    --exclude '__pycache__' --exclude 'config-3.14-*' --exclude site-packages \
    --exclude '_test*.so' --exclude 'xxlimited*.so' --exclude '_ssl.*' --exclude '_hashlib.*' --exclude '_sqlite3.*' \
    --exclude '_tkinter*' --exclude '_ctypes_test*' --exclude '_xxtestfuzz*' --exclude '_interp*' .) | (cd $H && tar -xf -)
cp $B/python/README.md $O/README.python-android.md
cp $PY/lib/python3.14/LICENSE.txt $O/LICENSE.python.txt
# bytecode made here (the first run on the phone would spend seconds on it, in a folder it may not write to); the
# unchecked-hash kind stays valid whatever mtimes the files get when the app unpacks them
HOSTPY=$(command -v python3.14 || true)
if [ -n "$HOSTPY" ]; then "$HOSTPY" -I -m compileall -q -j0 --invalidation-mode unchecked-hash $H >/dev/null
else echo "(no python3.14 here: no .pyc, the phone compiles them on first use)"; fi
"$STRIP" --strip-unneeded $L/*.so $(find $H -name '*.so')
echo "== $O"
du -sh $L/* $O/pyhome | sed "s|$ROOT/||"
