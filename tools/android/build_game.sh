#!/bin/bash
# tools/android/build_game.sh <game folder> [--apk] [--push]: the game code library for the Android app, from your own
# game files (v1.0.0; the folder holding exefs/, or exefs/ itself). On this computer: the generator (tools/gen.py,
# with the Android address shift) turns the ExeFS modules (runtime/modules.h) into C, the NDK compiles that into
# libeowgame.so (build/android/gamelib/arm64-v8a/), linked against the app's own libmain.so (build the app first:
# cd android && ./gradlew assembleDebug).
#   --apk   also build a personal APK with it inside (android/app/build/outputs/apk/debug/app-debug.apk)
#   --push  copy it into the app on the phone connected over adb (debug builds: run-as)
# The result holds code made from the game: keep it to yourself, never share it. (After LASRecomp-Android-Plus, MIT.)
set -euo pipefail
cd "$(dirname "$0")/../.."
ROOT=$PWD
GAME=${1:?usage: tools/android/build_game.sh <game folder> [--apk] [--push]}
shift
APK=0 PUSH=0
for a in "$@"; do case $a in --apk) APK=1 ;; --push) PUSH=1 ;; *) echo "unknown option $a"; exit 1 ;; esac; done

EXEFS=$GAME
[ -f "$EXEFS/main" ] || EXEFS=$GAME/exefs
[ -f "$EXEFS/main" ] && [ -f "$EXEFS/sdk" ] || { echo "no exefs/main and exefs/sdk in $GAME"; exit 1; }
EXEFS=$(cd "$EXEFS" && pwd)  # (the generator runs in tools/)
# v1.0.0 only (the build id at 0x40, as the start screen checks: runtime/ui/launcher.mm BUILD_ID)
WANT=$(sed -n 's/.*BUILD_ID\[8\] = {\([^}]*\)}.*/\1/p' runtime/ui/launcher.mm | tr -d ' ,' | sed 's/0x//g' | tr 'A-F' 'a-f')
HAVE=$(dd if="$EXEFS/main" bs=1 skip=64 count=8 2>/dev/null | xxd -p)
[ -z "$WANT" ] || [ "$WANT" = "$HAVE" ] || { echo "exefs/main is not version 1.0.0 (build id $HAVE)"; exit 1; }

SDK=${ANDROID_HOME:-$(sed -n 's/^sdk.dir=//p' android/local.properties 2>/dev/null || true)}
SDK=${SDK:-/opt/homebrew/share/android-commandlinetools}
NDK=${ANDROID_NDK_HOME:-$SDK/ndk/27.2.12479018}
case "$(uname -s)" in Darwin) HOST=darwin-x86_64; JOBS=$(sysctl -n hw.ncpu) ;; *) HOST=linux-x86_64; JOBS=$(nproc) ;; esac
CC="$NDK/toolchains/llvm/prebuilt/$HOST/bin/aarch64-linux-android30-clang"
[ -x "$CC" ] || { echo "Android NDK 27.2 not found ($NDK)"; exit 1; }
MAINLIB=$(ls -t android/app/build/intermediates/cxx/*/*/obj/arm64-v8a/libmain.so 2>/dev/null | head -1 || true)
[ -n "$MAINLIB" ] || { echo "build the app first: cd android && ./gradlew assembleDebug"; exit 1; }
PY=$ROOT/tools/venv/bin/python
[ -x "$PY" ] || { echo "no tools/venv (tools/macos/dev_deps.sh)"; exit 1; }

ID=$(tools/android/gamelib_id.sh)
FLAGS=$(tools/android/gamelib_id.sh --flags)
GENARGS=$(tools/android/gamelib_id.sh --gen-args)
GEN=build/android/gen
OBJ=build/android/gen-obj
OUT=build/android/gamelib/arm64-v8a
mkdir -p "$GEN" "$OBJ" "$OUT"

echo "== generating C (game code id $ID)"
# (unchanged chunks keep their files: a rebuild after a change compiles only what it touched)
MODS=$(sed -n 's/^ *X(\([a-z0-9]*\), *\(0x[0-9A-Fa-f]*\)ull).*/\1:\2/p' runtime/modules.h)
for mb in $MODS; do
    m=${mb%%:*} b=${mb#*:}
    (cd tools && "$PY" gen.py "$m" "$EXEFS/$m" "$b" "$ROOT/$GEN/$m" --hle ../runtime/hle/hle_symbols.txt $GENARGS)
done

echo "== compiling ($JOBS jobs)"
compile() {  # <source> <object>
    [ "$2" -nt "$1" ] && [ "$2" -nt runtime/cpu.h ] && [ "$2" -nt tools/android/gamelib_id.sh ] && return 0
    "$CC" $FLAGS -c "$1" -o "$2.tmp" && mv "$2.tmp" "$2"
}
export -f compile
export CC FLAGS
SRCS=$(for mb in $MODS; do ls "$GEN/${mb%%:*}"/*.c; done)
# the biggest first (no long file left alone at the end)
ls -S $SRCS | while read -r c; do echo "$c $OBJ/$(basename "$(dirname "$c")")_$(basename "$c" .c).o"; done |
    xargs -P "$JOBS" -n 2 bash -c 'compile "$0" "$1" || exit 255'
"$CC" $FLAGS -DGAMELIB_ID="\"$ID\"" -c tools/gamelib_entry.c -o "$OBJ/gamelib_entry.o"

# (objects of chunks the generator no longer writes - another split into files - would define functions twice)
for o in "$OBJ"/*_*.o; do
    b=$(basename "$o" .o); m=${b%%_*}
    [ "$b" = gamelib_entry ] && continue
    [ -e "$o" ] && [ ! -e "$GEN/$m/${b#*_}.c" ] && rm -f "$o"
done
echo "== linking"
# DT_NEEDED libmain.so: the runtime functions the game code calls are the app's own (it is loaded first)
"$CC" -shared -fuse-ld=lld -Wl,-z,max-page-size=16384 -Wl,-Bsymbolic -Wl,-soname,libeowgame.so \
    -o "$OUT/libeowgame.so.tmp" "$OBJ"/*.o -L"$(dirname "$MAINLIB")" -lmain
mv "$OUT/libeowgame.so.tmp" "$OUT/libeowgame.so"
echo "$ID" > "$OUT/libeowgame.so.id"
ls -la "$OUT/libeowgame.so"

if [ $APK = 1 ]; then
    echo "== personal APK"
    (cd android && ./gradlew assembleDebug -q -PeowGameLib="$ROOT/build/android/gamelib")
    ls -la android/app/build/outputs/apk/debug/app-debug.apk
fi
if [ $PUSH = 1 ]; then
    echo "== into the app on the phone"
    PKG=com.eowrecomp.android
    adb push "$OUT/libeowgame.so" "$OUT/libeowgame.so.id" /data/local/tmp/
    adb shell "run-as $PKG cp /data/local/tmp/libeowgame.so /data/local/tmp/libeowgame.so.id files/ &&
               rm /data/local/tmp/libeowgame.so /data/local/tmp/libeowgame.so.id"
    adb shell "run-as $PKG ls -la files/libeowgame.so files/libeowgame.so.id"
fi
