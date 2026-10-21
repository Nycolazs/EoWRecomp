# Building on macOS (Apple Silicon)

EoWRecomp runs on macOS through the same Vulkan backend as every other platform, on Apple's Metal via
[MoltenVK](https://github.com/KhronosGroup/MoltenVK). Tested on a MacBook Air M1 (8 GB) with macOS 27 and MoltenVK
1.4.2. (The Metal renderer inherited from LAS Recomp is still in the tree, `make MACOS_BACKEND=metal`, but it has
none of the Echoes of Wisdom fixes.)

## 1. Tools and libraries

Xcode's command line tools (`xcode-select --install`) and [Homebrew](https://brew.sh), then:

    tools/macos/dev_deps.sh

It installs SDL3, FFmpeg, MoltenVK, the Vulkan loader and headers, shaderc and Python 3.11 with Homebrew, fetches the
Vulkan Memory Allocator header (pinned, checksum checked) into `build/deps-macos`, and makes `tools/venv` with the
recompiler's Python modules (capstone, unicorn, lz4, cryptography, pillow).

## 2. Your own game files

You need your own *Echoes of Wisdom* **v1.0.0** (title `01008CF01BAAC000`, no update) as an `.nsp` or the program NCA,
and the `prod.keys` of your own Switch. Nothing of the game is in this repository, and nothing tells you where to get
it.

    tools/venv/bin/python -I tools/nca_extract.py <prod.keys> <your game.nsp> extracted --no-romfs --romfs-image

This writes `extracted/exefs/` (the code, ~40 MB) and `extracted/romfs.bin` (the data, 7.5 GB, the original image).
The extractor refuses a truncated package (a copy that didn't finish): the code sits at its end.

Alternatively, the start screen sets the game up from the package itself: choose the folder with the `.nsp` / `.xci`
and `prod.keys` - only the ExeFS is written out, and the data is read straight from your package
(`runtime/nxpkg.c`), so there is no 7.5 GB copy. Keep the package where it is afterwards.

## 3. Build

    make gen                 # the game code as C (generated/, a few minutes)
    make -j8                 # build/eowrecomp and build/eowgame.dylib

The first build of the game code compiles ~300,000 functions and takes a while on 8 cores; later builds only
recompile what changed. `FNPROF=1` builds the game code with the function profiler's hook (`EOWRECOMP_FNPROF`).

## 4. Run

From the repository (it finds `extracted/` there):

    build/eowrecomp

Useful environment variables (all `EOWRECOMP_*`): `EOWRECOMP_SHOT_AT=300,600` (screenshots into `build/frames/`),
`EOWRECOMP_INPUT="1550:STICK_UP:150,1820:A:10"` (scripted input), `EOWRECOMP_NO_LAUNCHER=1` (straight into the game),
`EOWRECOMP_VK_VALIDATION=1` (Khronos validation layer, `brew install vulkan-validationlayers`).

## Tests that need no game files

    tools/test_nxpkg.sh                  # the package reader: AES / XTS / LZ4 against reference vectors
    tools/test_translate.sh 8000 8 glsl  # the shader translator: random programs -> GLSL -> SPIR-V, validated
    tools/test_moltenvk.sh 8000 8        # ... and on through SPIRV-Cross to MSL, compiled by Metal (MoltenVK's path)
