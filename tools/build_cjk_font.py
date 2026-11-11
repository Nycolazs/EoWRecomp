"""Build a static, openly licensed TrueType Chinese fallback (no console font data).

Source/version pins follow S2Arm's pack/fonts/make_open_fonts.py. Requires fonttools==4.67.0.
"""
from pathlib import Path
import argparse
import hashlib
import urllib.request
from importlib.metadata import version

REV = '45e3d5269e4a17d823f6d84470bc084dc5ae46f6'
BASE = f'https://raw.githubusercontent.com/google/fonts/{REV}/ofl/notosanssc/'
SHA = 'a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da'
OFL_SHA = '1c05c68c34f9708415aada51f17e1b0092d2cea709bf4a94cd38114f9e73d7d9'


def fetch(path, url, digest):
    if not path.exists():
        urllib.request.urlretrieve(url, str(path) + '.part')
        Path(str(path) + '.part').replace(path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise SystemExit(f'{path}: source checksum mismatch')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cache', type=Path, required=True)
    ap.add_argument('--out', type=Path, default=Path(__file__).resolve().parents[1] / 'third_party/fonts/NotoSansSC')
    args = ap.parse_args()
    if version('fonttools') != '4.67.0':
        raise SystemExit('requires fonttools==4.67.0')
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    from fontTools.subset import Options, Subsetter
    args.cache.mkdir(parents=True, exist_ok=True)
    args.out.mkdir(parents=True, exist_ok=True)
    source = args.cache / 'NotoSansSC-variable.ttf'
    license_file = args.cache / 'NotoSansSC-OFL.txt'
    fetch(source, BASE + 'NotoSansSC%5Bwght%5D.ttf', SHA)
    fetch(license_file, BASE + 'OFL.txt', OFL_SHA)
    font = instantiateVariableFont(TTFont(source), {'wght': 400}, updateFontNames=False)
    # Match the small shared-font table set used by S2Arm: no GPOS/BASE or variable metadata;
    # retain vertical substitutions and all encoded characters, not unreferenced layout alternates.
    options = Options()
    options.layout_features = ['vert', 'vrt2']
    options.drop_tables += ['GPOS', 'BASE', 'STAT', 'DSIG']
    options.name_IDs = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14]
    options.name_languages = ['*']
    options.notdef_outline = True
    options.glyph_names = False
    options.recalc_timestamp = False
    subsetter = Subsetter(options)
    subsetter.populate(unicodes=font.getBestCmap())
    subsetter.subset(font)
    font.recalcTimestamp = False
    font['head'].created = font['head'].modified = 3857241600
    if 'DSIG' in font:
        del font['DSIG']
    names = {1: 'EoW Sans SC', 2: 'Regular', 3: 'EoW Sans SC Regular 2.004', 4: 'EoW Sans SC Regular',
             6: 'EoWSansSC-Regular', 16: 'EoW Sans SC', 17: 'Regular'}
    for record in list(font['name'].names):
        if record.nameID in names:
            font['name'].setName(names[record.nameID], record.nameID, record.platformID, record.platEncID, record.langID)
    out = args.out / 'EowSansSC-Regular.ttf'
    font.save(out)
    # Preserve the license text, normalizing its upstream trailing spaces.
    license_text = b'\n'.join(line.rstrip(b' \t') for line in license_file.read_bytes().splitlines()) + b'\n'
    (args.out / 'OFL.txt').write_bytes(license_text)
    print(out, out.stat().st_size, hashlib.sha256(out.read_bytes()).hexdigest())


if __name__ == '__main__':
    main()
