#!/bin/bash
# tools/android/ondevice/check_public_apk.sh <apk>: an APK to share carries no code or data made from the game
# (docs/BUILD-android.md). Fails when it finds:
#   - libeowgame.so (the game code library) or any other library defining recompiled game functions (f_70xxxxxxxx)
#   - generated C (the generator's chunk files: "#include \"cpu.h\"" + f_70... functions) inside the APK or inside assets/ondevice.zip
#   - game files (exefs/, romfs/, main.npdm, .nso / .nca / .nsp / .xci)
# and shows what it carries for building the game code on the phone.
set -euo pipefail
APK=${1:?usage: check_public_apk.sh <apk>}
cd "$(dirname "$0")/../../.."
SDK=${ANDROID_HOME:-$(sed -n 's/^sdk.dir=//p' android/local.properties 2>/dev/null || true)}
[ -n "$SDK" ] && [ -d "$SDK/ndk" ] || SDK=$(sed -n 's/^sdk.dir=//p' "$(git rev-parse --git-common-dir)/../android/local.properties")
NM=$(ls "$SDK"/ndk/27.2.12479018/toolchains/llvm/prebuilt/*/bin/llvm-nm | head -1)
W=build/ondevice/obj/apkcheck
rm -rf $W
mkdir -p $W/apk $W/zip
unzip -q -o "$APK" -d $W/apk
bad=0
fail() { echo "FAIL: $*"; bad=1; }
find $W/apk -name 'libeowgame.so*' | grep -q . && fail "libeowgame.so in the APK"
find $W/apk \( -iname '*.nso' -o -iname '*.nca' -o -iname '*.nsp' -o -iname '*.xci' -o -name main.npdm -o -type d -name exefs \
    -o -type d -name romfs \) | grep -q . && fail "game files in the APK"
for so in $(find $W/apk -name '*.so'); do
    n=$("$NM" -D --defined-only "$so" 2>/dev/null | grep -cE ' f_70[0-9a-f]{8}$' || true)
    [ "$n" = 0 ] || fail "$(basename "$so") defines $n recompiled game functions"
done
if [ -f $W/apk/assets/ondevice.zip ]; then
    unzip -q -o $W/apk/assets/ondevice.zip -d $W/zip
    echo "assets/ondevice.zip: $(find $W/zip -type f | wc -l | tr -d ' ') files"
fi
grep -rlE '^(COLD_FN )?void f_70[0-9a-f]{8}\(Cpu \*CPU_RESTRICT c\) \{' $W/apk $W/zip 2>/dev/null | grep -v '/tools/gen.py$' |
    grep -q . && fail "generated C: $(grep -rlE '^(COLD_FN )?void f_70[0-9a-f]{8}\(Cpu \*CPU_RESTRICT c\) \{' $W/apk $W/zip | grep -v '/tools/gen.py$' | head -3)"
echo "native libraries:"
(cd $W/apk/lib/arm64-v8a && ls -l | awk 'NR > 1 {printf "  %-24s %10d\n", $9, $5}')
ls $W/apk/assets/licenses >/dev/null 2>&1 && echo "licenses: $(ls $W/apk/assets/licenses | tr '\n' ' ')"
rm -rf $W
[ $bad = 0 ] && echo "OK: no game code, generated C or game files in $(basename "$APK") ($(du -h "$APK" | cut -f1))"
exit $bad
