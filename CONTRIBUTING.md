# Contributing to EoWRecomp

Bug reports, testing on different hardware and code contributions are welcome. This project is developed mostly with
AI; contributions written by hand, with AI assistance or anything in between are equally welcome - what matters is
that a change works, is tested, and follows the rules below (the full list is in [CLAUDE.md](CLAUDE.md)).

## The one hard rule: no Nintendo files

Nothing owned by Nintendo may ever be committed or attached to an issue or pull request: no copies of the game in any
form, no keys, no extracted game files (`extracted/`), no generated code (`generated/`), no fonts, no game assets of
any kind (textures, models, music, text, level files) - also not "just one file for testing". Screenshots and short
clips in bug reports are fine.

## Project layout

- `tools/nca_extract.py` - ExeFS / RomFS from the program NCA of the user's own dump (handles compressed RomFS).
- `tools/gen.py` - the recompiler: the game's executables to C (`generated/`, never committed). `tools/patches.py`
  holds code patches applied during generation; every patch site checks the original instruction word.
- `runtime/` - the host side: `kernel.c`, `loader.c`, `ipc.c`, `services/` (the Switch OS replacement), `hle/`
  (host replacements of SDK functions), `nvn/` (NVN, the Vulkan and Metal backends, the Maxwell shader translator),
  `ui/` (ImGui overlay menu and start screen).

## Testing your change

- Build cleanly on your platform. If you touch shared rendering code, say which backend you tested.
- A fix isn't verified until it has been seen working (screenshot, log or test). Scripted runs (`tools/shot.sh`,
  `EOWRECOMP_*` environment hooks) save a screenshot at a given frame.

## License

By contributing, you agree that your contributions are licensed under the project's [MIT License](LICENSE).
