#!/bin/bash
# tools/android/ondevice/build_all.sh: everything an APK that builds the game code on the phone carries
# (docs/BUILD-android.md), into build/ondevice/stage/jniLibs (programs and libraries) and build/ondevice/assets
# (ondevice.zip, licenses/). Then: cd android && ./gradlew assembleRelease -PonDeviceBuild=true
set -euo pipefail
D=$(dirname "$0")
"$D/build_python.sh"
"$D/build_zig.sh"
"$D/build_cckit.sh"
"$D/make_assets.sh"
