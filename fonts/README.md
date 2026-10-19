# System fonts

The game draws its text with the Switch's built-in system font, which isn't part of the game files.
Without it, LAS Recomp uses Hiragino Sans (built into macOS, the same style of Japanese gothic as the Switch's
Morisawa UD Shin Go), then Arial.

For the exact look, put your own copies of the system fonts here:

- `nintendo_udsg-r_std_003.bfttf`: the main font (a `.ttf` with the same name, or spelled `nintendo_udsgr_std_003`, also works)
- `nintendo_ext_003.bfttf`: button glyphs (optional)

`LARECOMP_FONT=<file>` overrides everything (.ttf / .otf / .ttc / .bfttf).
