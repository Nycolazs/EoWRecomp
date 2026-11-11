# EoW Sans SC Regular

An openly licensed Chinese fallback, built from Noto Sans SC 2.004 (Google Fonts, SIL OFL 1.1).
This is font data from the open font project, not a console font.

The pinned source, SHA-256 and deterministic static TrueType conversion are in `tools/build_cjk_font.py`.
The conversion fixes weight 400, removes variation tables and renames the family to EoW Sans SC.
Run with `fonttools==4.67.0`:

```sh
python tools/build_cjk_font.py --cache <private-download-cache>
python tools/test_cjk_font.py
```

Source: https://github.com/google/fonts/tree/45e3d5269e4a17d823f6d84470bc084dc5ae46f6/ofl/notosanssc
The accompanying `OFL.txt` retains the original copyright and license.
Source/version selection follows S2Arm's `pack/fonts/make_open_fonts.py`; this small builder does not include
S2Arm's full set of system-font replacements or controller glyph generator.
