# Android

The Android app (`android/`, package `com.eowrecomp.android`) is the same runtime as on the computer - kernel,
services, NVN on Vulkan, the start screen and menus - built with the NDK for arm64 phones (Android 11+, Vulkan 1.3),
plus touch controls. Its structure follows [LASRecomp-Android-Plus](https://github.com/Nycolazs/LASRecomp-Android-Plus)
(MIT).

The APK holds **no game code and no game data**. The game code is built from your own game files on a computer and
packed into a personal APK that you keep to yourself.

## Build (on a Mac or Linux computer)

Android SDK with NDK 27.2 (`sdkmanager 'ndk;27.2.12479018' 'platforms;android-36'`), Java 17+, and the macOS setup
of [BUILD-macos.md](BUILD-macos.md) (or Linux's) for the recompiler's Python.

    tools/android/build_deps.sh                  # FFmpeg, shaderc, Vulkan headers, VMA, volk, SDL3 (once, ~15 min)
    (cd android && ./gradlew assembleDebug)       # the app: libmain.so (the runtime) + SDL3
    tools/android/build_game.sh extracted --apk   # the game code (libeowgame.so) from your exefs/, and the APK with it

`build_game.sh` runs the generator with the Android address shift (the guest layout sits 0x6000000000 lower there:
`runtime/cpu.h` `GUEST_SHIFT`) and compiles with the NDK at -O2. The result is
`android/app/build/outputs/apk/debug/app-debug.apk`; `--push` copies a new game library into an installed debug app
over adb instead.

## On the phone

1. Copy your `.nsp` (or `.xci`) and your `prod.keys` into one folder, e.g. `Download/EoW`.
2. Open the app: **Game files folder** -> pick that folder -> **Set up game files**. The keys are copied into the
   app; the package is **not** copied: the game reads its data straight from it through the folder's permission
   (so keep it there). Only the game's code files (~40 MB) are written out.
3. Play with the on-screen controls or a Bluetooth / USB controller (the touch controls hide while a controller is
   used and come back with a touch). The menu button at the top centre, or Android's Back button, opens the settings.

The app keeps its files in `Android/data/com.eowrecomp.android/files/` (saves in `save/`, the log in
`eowrecomp.log`). Test and debug variables go into `env.txt` there, one `EOWRECOMP_...=value` per line.

## State

Checked on an Android 15 emulator (arm64, gfxstream Vulkan): the start screen, landscape, the folder picker, the
package read through its document, going to the background and back. Not yet run on a phone with the game.
