#!/bin/bash
# tools/android/ondevice/build_cckit.sh: what compiling + linking the game code on the phone needs besides the compiler
# itself (docs/BUILD-android.md), taken from the NDK on this computer, into build/ondevice/stage/cckit/:
#   sysroot/usr/include/...   the few bionic headers the generated C and tools/gamelib_entry.c include (found from a
#                             probe's dependency list, as tools/zig_slim.sh does for Windows)
#   sysroot/usr/lib/aarch64-linux-android/30/  crtbegin_so.o, crtend_so.o, the libc / libm / libdl stubs
#   clang/include/...         the compiler's own headers they include (stddef.h, stdint.h, float.h, limits.h + pieces)
#   clang/lib/libclang_rt.builtins-aarch64-android.a  (the NDK's: outline atomics etc.)
#   gamelib-src/              cpu.h simd.h simd_fp.h gamelib.h gamelib_entry.c hle_symbols.txt (runtime/gamebuild.c's
#                             resource names)
#   link.rsp                  the ld.lld flags of build_game.sh's link (NDK clang -### of it), minus objects / output
# The compiler's own headers must be those of the compiler used on the phone: HOSTZIG=<zig of the same version for this
# computer> finds them with it (default: build_zig.sh's build/ondevice/zig-host); without it, the NDK clang 18's.
set -euo pipefail
cd "$(dirname "$0")/../../.."
SDK=${ANDROID_HOME:-$(sed -n 's/^sdk.dir=//p' android/local.properties 2>/dev/null || true)}
[ -n "$SDK" ] && [ -d "$SDK/ndk" ] || SDK=$(sed -n 's/^sdk.dir=//p' "$(git rev-parse --git-common-dir)/../android/local.properties")
NDK=${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}
case "$(uname -s)" in Darwin) HOST=darwin-x86_64 ;; *) HOST=linux-x86_64 ;; esac
TC=$NDK/toolchains/llvm/prebuilt/$HOST
CC=$TC/bin/aarch64-linux-android30-clang
SYS=$TC/sysroot
RES=$(ls -d "$TC"/lib/clang/*/ | head -1)
CLANG_INC=${RES}include
PROBE=("$CC")
[ -n "${HOSTZIG:-}" ] || [ ! -x build/ondevice/zig-host/zig ] || HOSTZIG=$PWD/build/ondevice/zig-host/zig  # (build_zig.sh's)
if [ -n "${HOSTZIG:-}" ]; then
    CLANG_INC=$(cd "$(dirname "$HOSTZIG")/lib/include" && pwd)
    PROBE=("$HOSTZIG" clang -target aarch64-linux-android30 -nostdinc -isystem "$CLANG_INC" -isystem "$SYS/usr/include"
           -isystem "$SYS/usr/include/aarch64-linux-android")
fi
O=build/ondevice/stage/cckit W=build/ondevice/obj/cckit
rm -rf $O $W
mkdir -p $O/sysroot/usr/lib/aarch64-linux-android/30 $O/clang/include $O/clang/lib $O/gamelib-src $W
FLAGS="$(tools/android/gamelib_id.sh --flags)"

printf '#include "cpu.h"\n#include "simd.h"\n' > $W/probe.c
"${PROBE[@]}" $FLAGS -c $W/probe.c -o $W/probe.o -MD -MF $W/probe.d
"${PROBE[@]}" $FLAGS '-DGAMELIB_ID="0"' -c tools/gamelib_entry.c -o $W/entry.o -MD -MF $W/entry.d
n=0
for h in $(cat $W/probe.d $W/entry.d | tr ' ' '\n' | grep '\.h$' | sort -u); do
    case $h in
        */sysroot/*) rel=sysroot/${h#*/sysroot/}; h=$SYS/${h#*/sysroot/} ;;  # (the .d paths: bin/../sysroot)
        "$CLANG_INC"/*) rel=clang/include/${h#"$CLANG_INC"/} ;;
        */lib/clang/*/include/*) rel=clang/include/${h#*/include/} ;;
        *) continue ;;
    esac
    mkdir -p "$O/$(dirname "$rel")"
    cp "$h" "$O/$rel"
    n=$((n + 1))
done
L=$SYS/usr/lib/aarch64-linux-android
cp $L/30/crtbegin_so.o $L/30/crtend_so.o $L/30/libc.so $L/30/libm.so $L/30/libdl.so $O/sysroot/usr/lib/aarch64-linux-android/30/
cp "${RES}lib/linux/libclang_rt.builtins-aarch64-android.a" $O/clang/lib/
cp runtime/cpu.h runtime/simd.h runtime/simd_fp.h runtime/gamelib.h runtime/modules.h tools/gamelib_entry.c \
   runtime/hle/hle_symbols.txt $O/gamelib-src/
# the link (tools/android/build_game.sh), as NDK clang runs ld.lld for it. On the phone:
#   libzig.so ld.lld @cckit/link.rsp -o libeowgame.so cckit/sysroot/usr/lib/aarch64-linux-android/30/crtbegin_so.o
#       <objects> -L<nativeLibraryDir> -lmain -Lcckit/sysroot/usr/lib/aarch64-linux-android/30
#       cckit/clang/lib/libclang_rt.builtins-aarch64-android.a -ldl -lc cckit/sysroot/.../30/crtend_so.o
cat > $O/link.rsp <<'EOF'
-EL
--fix-cortex-a53-843419
-z now
-z relro
--hash-style=gnu
--eh-frame-hdr
-m aarch64linux
-shared
-z max-page-size=16384
-Bsymbolic
-soname libeowgame.so
EOF
echo "$O: $n headers, $(find $O -type f | wc -l | tr -d ' ') files, $(du -sh $O | cut -f1)"
