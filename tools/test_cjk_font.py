"""Verify the fallback contains real glyphs and static TrueType outlines the game can read."""
from pathlib import Path
import sys
from fontTools.ttLib import TTFont

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'third_party/fonts/NotoSansSC/EowSansSC-Regular.ttf'
font = TTFont(path)
assert font.sfntVersion == '\x00\x01\x00\x00' and 'glyf' in font, 'requires TrueType outlines'
assert 'fvar' not in font and 'CFF ' not in font and 'CFF2' not in font, 'requires a static font'
cmap = font.getBestCmap()
needed = '挥剑保存智慧再现挥劍儲存裝備道具A0123'
missing = sorted(set(ch for ch in needed if ord(ch) not in cmap))
assert not missing, f'missing CJK glyphs: {"".join(missing)}'
for ch in needed:
    assert font['glyf'][cmap[ord(ch)]].numberOfContours != 0, f'empty glyph: {ch}'
print(f'PASS: {len(set(needed))} prompt/UI glyphs; static TrueType; {len(cmap)} mapped characters')
