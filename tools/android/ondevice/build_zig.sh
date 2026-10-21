#!/bin/bash
# tools/android/ondevice/build_zig.sh: the compiler for building the game code ON the phone (docs/BUILD-android.md):
# zig's aarch64-linux release - one static binary (musl inside, no bionic needed), which runs on Android as is and
# holds clang + lld (`zig clang`, `zig ld.lld`: nothing else of zig's lib/ folder is read). Same pinned version as the
# desktop builds (tools/fetch_zig.sh VER / sha256). Into build/ondevice/stage/:
#   jniLibs/arm64-v8a/libzig.so   (a program named lib*.so: Android runs programs only from the native library folder)
#   LICENSE.zig.txt
# and this computer's zig of the same version at build/ondevice/zig-host/ (tools/android/ondevice/build_cckit.sh
# HOSTZIG: the compiler headers that clang version needs).
set -euo pipefail
cd "$(dirname "$0")/../../.."
VER=$(sed -n 's/^VER=//p' tools/fetch_zig.sh)
pinned() { sed -n "s/^ *$1) *SHA=\([0-9a-f]*\).*/\1/p" tools/fetch_zig.sh; }
case "$(uname -s)-$(uname -m)" in Darwin-arm64) HT=aarch64-macos ;; Linux-x86_64) HT=x86_64-linux ;; Linux-aarch64) HT=aarch64-linux ;;
                                  *) echo "no pinned zig for this computer"; exit 1 ;; esac
B=build/ondevice
get() {  # <target> <dir>: the release archive, checked against tools/fetch_zig.sh's sha256, unpacked into <dir>
    local name=zig-$1-$VER sha; sha=$(pinned "$1")
    mkdir -p $B/zig-dl
    [ -f $B/zig-dl/$name.tar.xz ] || curl -fL --proto '=https' -o $B/zig-dl/$name.tar.xz https://ziglang.org/download/$VER/$name.tar.xz
    [ "$(shasum -a 256 $B/zig-dl/$name.tar.xz | cut -d' ' -f1)" = "$sha" ] || { echo "$name.tar.xz: sha256 mismatch"; exit 1; }
    if [ ! -x "$2/zig" ]; then
        rm -rf "$2" $B/zig-dl/$name
        tar -xJf $B/zig-dl/$name.tar.xz -C $B/zig-dl
        mv $B/zig-dl/$name "$2"
    fi
}
get aarch64-linux $B/zig
get $HT $B/zig-host
mkdir -p $B/stage/jniLibs/arm64-v8a
cp $B/zig/zig $B/stage/jniLibs/arm64-v8a/libzig.so
cp $B/zig/LICENSE $B/stage/LICENSE.zig.txt
echo "libzig.so: zig $VER, $(du -h $B/stage/jniLibs/arm64-v8a/libzig.so | cut -f1)"
