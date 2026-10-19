# EoWRecomp

**EoWRecomp** is a work-in-progress static recompilation of *The Legend of Zelda: Echoes of Wisdom* (Nintendo Switch,
2024) into a native PC application. It is **not an emulator**: the game's ARM64 code is translated to C at build time,
and a runtime replaces the Switch operating system (Horizon) and its graphics API (NVN), rendering with Vulkan.

> [!WARNING]
> Early development. Nothing is playable yet; see [docs/STATUS.md](docs/STATUS.md) for the current milestone.

> [!NOTE]
> **No game files are included.** EoWRecomp needs your own legally obtained copy of *Echoes of Wisdom* **v1.0.0**
> (title 01008CF01BAAC000, no updates) and the keys from your own console. Instructions on how to obtain these files
> will **not** be provided. The game code is recompiled locally from your own files; nothing from the game ships with
> this repository or its builds.

This project is developed almost entirely with AI (Anthropic's Claude, through Claude Code), directed and tested by a
human - the same way as the project it is based on.

## Based on LAS Recomp

EoWRecomp is a fork of **[LAS Recomp](https://github.com/peiikai/LASRecomp)** by **Peiikai** and its contributors
(MIT License), the static recompilation of *Link's Awakening* (Switch, 2019). Both games are by Grezzo, and the
recompiler, the Horizon runtime, the NVN implementation, the Maxwell shader translator and the Vulkan / Metal renderers
come from LAS Recomp. Huge thanks to its author and contributors. Its Link's Awakening-specific parts (randomizer,
gameplay patches) are not part of this fork.

## Building

Not ready yet. Planned targets: Windows x86-64 + Vulkan 1.3 first, Linux x86-64 (same backend), later optionally macOS
on Apple Silicon (Metal). See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for how the pipeline works.

## Credits

- **LAS Recomp** by Peiikai and contributors - the base of this project (MIT, https://github.com/peiikai/LASRecomp)
- [SDL 3](https://libsdl.org) - windowing, input, audio (zlib license)
- [FFmpeg](https://ffmpeg.org) - movie playback (LGPL 2.1+, linked dynamically)
- [Dear ImGui](https://github.com/ocornut/imgui) - overlay menu and start screen (MIT)
- [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) - Vulkan renderer (MIT)
- [shaderc](https://github.com/google/shaderc) - GLSL to SPIR-V at run time (Apache 2.0)
- [SMAA](https://github.com/iryoku/smaa) by Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro and Diego
  Gutierrez - anti-aliasing option (MIT, `third_party/smaa/LICENSE.txt`)
- [AMD FidelityFX Super Resolution 1](https://github.com/GPUOpen-Effects/FidelityFX-FSR) - upscaler option, ported to
  GLSL (MIT)
- [Baloo 2](https://github.com/EkType/Baloo2) by Ek Type - start screen font (SIL Open Font License 1.1,
  `third_party/fonts/Baloo2/OFL.txt`)
- [Capstone](https://www.capstone-engine.org) (BSD 3-clause) - disassembly in the recompiler
- [Unicorn](https://www.unicorn-engine.org) (GPL 2.0; Python bindings BSD) - runs small pieces of code at build time
  to resolve jump tables
- [python-lz4](https://github.com/python-lz4/python-lz4) (BSD 3-clause; LZ4: BSD 2-clause) - NSO and NCA decompression
- [cryptography](https://cryptography.io) (Apache 2.0 / BSD) - NCA decryption in `tools/nca_extract.py`
- [envytools](https://github.com/envytools/envytools) - NVIDIA GPU documentation used by the shader translator
- The Switch homebrew community's documentation ([Switchbrew](https://switchbrew.org)) and Atmosphère's file system
  sources (NCA / BucketTree compression layout)

## Legal

EoWRecomp is a fan project. It is not affiliated with, endorsed by or sponsored by Nintendo. *The Legend of Zelda* and
*Echoes of Wisdom* are trademarks of Nintendo. No game files, game code, assets, keys or fonts are included - not in
this repository and not in builds. You must provide your own legally obtained copy of the game to use it.

## License

EoWRecomp's own code is open source under the [MIT License](LICENSE) (which keeps LAS Recomp's copyright notice).
Third-party components keep their own licenses, listed above. The license covers EoWRecomp's code only - it grants
nothing for Nintendo's game, which you must supply yourself.
