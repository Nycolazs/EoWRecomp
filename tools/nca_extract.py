"""Extract ExeFS and RomFS from a program NCA (NCA3), including compressed sections.

Usage: nca_extract.py <prod.keys> <program.nca> <outdir> [--no-romfs] [--info]

hactool (SciresM) can't read this game's program NCA: its RomFS section is stored through the BucketTree
compression layer (LZ4 blocks, firmware 12+), and hactool aborts on it before writing even the ExeFS. Layout as in
Atmosphere's fssystem (NcaFileSystemDriver, CompressedStorage, BucketTree) and Switchbrew's NCA page:
  AES-CTR decrypted section -> IVFC / SHA-256 hash layers -> CompressedStorage -> RomFS / PFS0.
The compression layer sits on top of the integrity layer: its table offset and its physical offsets are relative to
the hash layers' data region, and its virtual space is the file system itself.
Hashes are not verified. Writes <outdir>/exefs/<files> and <outdir>/romfs/<tree>.
"""
import argparse
import os
import struct
import sys

import lz4.block
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes


def load_keys(path):
    keys = {}
    for line in open(path):
        if '=' in line:
            k, v = line.split('=', 1)
            keys[k.strip()] = v.strip()
    return keys


def aes_ecb_dec(key, data):
    d = Cipher(algorithms.AES(key), modes.ECB()).decryptor()
    return d.update(data) + d.finalize()


class Nca:
    def __init__(self, path, keys):
        self.f = open(path, 'rb')
        raw = self.f.read(0xC00)
        hk = bytes.fromhex(keys['header_key'])
        hdr = b''
        for s in range(6):  # AES-XTS, 0x200 sectors, Nintendo's big-endian sector tweak
            d = Cipher(algorithms.AES(hk), modes.XTS(s.to_bytes(16, 'big'))).decryptor()
            hdr += d.update(raw[s * 0x200:(s + 1) * 0x200]) + d.finalize()
        if hdr[0x200:0x204] != b'NCA3':
            raise SystemExit(f'{path}: not an NCA3 (wrong header_key?)')
        self.hdr = hdr
        self.content_type = hdr[0x205]
        self.title_id = struct.unpack_from('<Q', hdr, 0x210)[0]
        self.sdk = struct.unpack_from('<I', hdr, 0x21C)[0]
        gen = max(hdr[0x206], hdr[0x220])
        self.keygen = gen - 1 if gen else 0
        if any(hdr[0x230:0x240]):
            raise SystemExit('rights ID set: titlekey crypto is not supported (XCI program NCAs use the key area)')
        kaek = bytes.fromhex(keys[f'key_area_key_application_{self.keygen:02x}'])
        self.key_area = aes_ecb_dec(kaek, hdr[0x300:0x340])
        self.sections = []
        for i in range(4):
            st, en = struct.unpack_from('<II', hdr, 0x240 + i * 16)
            if st == en == 0:
                self.sections.append(None)
                continue
            self.sections.append(Section(self, i, st * 0x200, (en - st) * 0x200, hdr[0x400 + i * 0x200:0x600 + i * 0x200]))


class Section:
    """One FS section: raw reads with AES-CTR decryption."""

    def __init__(self, nca, idx, offset, size, fh):
        self.nca, self.idx, self.offset, self.size, self.fh = nca, idx, offset, size, fh
        self.version, self.fs_type, self.hash_type, self.enc_type = struct.unpack_from('<HBBB', fh, 0)
        self.ctr_hi = fh[0x140:0x148][::-1]  # generation + secure value, as the IV's upper half
        self.key = nca.key_area[0x20:0x30]  # AES-CTR key (index 2)
        if any(fh[0x148:0x178]) and struct.unpack_from('<Q', fh, 0x150)[0]:
            raise SystemExit(f'section {idx}: sparse storage is not supported')
        self.comp_off, self.comp_size = struct.unpack_from('<QQ', fh, 0x178)
        self.comp_header = fh[0x188:0x198]

    def read_raw(self, off, size):
        """Decrypted section bytes [off, off+size)."""
        a = off & ~0xF
        self.nca.f.seek(self.offset + a)
        data = self.nca.f.read(off + size - a)
        if self.enc_type == 1:  # none
            return data[off - a:]
        if self.enc_type != 3:
            raise SystemExit(f'section {self.idx}: encryption type {self.enc_type} not supported')
        iv = self.ctr_hi + ((self.offset + a) >> 4).to_bytes(8, 'big')
        d = Cipher(algorithms.AES(self.key), modes.CTR(iv)).decryptor()
        return (d.update(data) + d.finalize())[off - a:]


class CompressedStorage:
    """BucketTree of CompressedStorage::Entry {s64 virt; s64 phys; u8 type; s8 level; u16; u32 phys_size} over a
    section; node size 0x4000. Types: 0 none (copy), 1 zeros, 3 LZ4."""
    NODE = 0x4000
    ENTRY = 0x18

    def __init__(self, sec, base):
        self.sec, self.base = sec, base
        magic, ver, count = struct.unpack_from('<4sIi', sec.comp_header, 0)
        if magic != b'BKTR':
            raise SystemExit(f'section {sec.idx}: bad compression table header {magic!r}')
        table = sec.read_raw(base + sec.comp_off, sec.comp_size)
        per_set = (self.NODE - 0x10) // self.ENTRY
        sets = (count + per_set - 1) // per_set
        per_node = (self.NODE - 0x10) // 8
        l2 = 0 if sets <= per_node else (sets - (per_node - 0) + per_node - 2) // (per_node - 1)
        node_storage = (1 + l2) * self.NODE
        entries = []
        for s in range(sets):
            base = node_storage + s * self.NODE
            idx, n, end = struct.unpack_from('<iiq', table, base)
            if idx != s:
                raise SystemExit(f'compression table: entry set {s} has index {idx} (node storage size wrong?)')
            for e in range(n):
                entries.append(struct.unpack_from('<qqbbHI', table, base + 0x10 + e * self.ENTRY))
            self.end = end
        if len(entries) != count:
            raise SystemExit(f'compression table: {len(entries)} entries, header says {count}')
        self.entries = entries
        self.virts = [e[0] for e in entries]
        self.cached = None

    def read(self, off, size):
        import bisect
        out = bytearray()
        i = bisect.bisect_right(self.virts, off) - 1
        while size > 0:
            v, p, typ, _lvl, _r, psize = self.entries[i]
            vend = self.virts[i + 1] if i + 1 < len(self.entries) else self.end
            if typ == 0:
                block = self.sec.read_raw(self.base + p + (off - v), min(size, vend - off))
                skip = 0
            elif typ == 1:
                block, skip = bytes(vend - v), off - v
            elif typ == 3:  # (small RomFS files share a block: keep the last one)
                if self.cached is None or self.cached[0] != i:
                    self.cached = (i, lz4.block.decompress(self.sec.read_raw(self.base + p, psize), uncompressed_size=vend - v))
                block, skip = self.cached[1], off - v
            else:
                raise SystemExit(f'compression type {typ} not supported')
            take = block[skip:skip + min(size, vend - off)]
            out += take
            off += len(take)
            size -= len(take)
            i += 1
        return bytes(out)


class View:
    """The file system's bytes: the hash layers' data region, through the compression layer when there is one."""

    def __init__(self, sec):
        self.sec = sec
        self.base, _ = data_region(sec)
        self.comp = CompressedStorage(sec, self.base) if sec.comp_size else None

    def read(self, off, size):
        return self.comp.read(off, size) if self.comp else self.sec.read_raw(self.base + off, size)


def data_region(sec):
    """(offset, size) of the payload (PFS0 / RomFS) under the hash layers."""
    fh = sec.fh
    if sec.hash_type == 2:  # HierarchicalSha256: master hash, block size, layer count, layer regions
        layers = struct.unpack_from('<I', fh, 0x8 + 0x24)[0]
        return struct.unpack_from('<QQ', fh, 0x8 + 0x28 + (layers - 1) * 16)
    if sec.hash_type == 3:  # HierarchicalIntegrity (IVFC): 6 levels, the last is the data
        magic = fh[0x8:0xC]
        if magic != b'IVFC':
            raise SystemExit(f'section {sec.idx}: bad IVFC magic {magic!r}')
        levels = struct.unpack_from('<I', fh, 0x8 + 0xC)[0]
        o = 0x8 + 0x10 + (levels - 2) * 0x18
        off, size = struct.unpack_from('<QQ', fh, o)
        return off, size
    raise SystemExit(f'section {sec.idx}: hash type {sec.hash_type} not supported')


def extract_pfs0(view, base, outdir):
    magic, n, strsz = struct.unpack_from('<4sII', view.read(base, 0x10))
    if magic != b'PFS0':
        raise SystemExit(f'not a PFS0: {magic!r}')
    meta = view.read(base, 0x10 + n * 0x18 + strsz)
    strtab = 0x10 + n * 0x18
    os.makedirs(outdir, exist_ok=True)
    names = []
    for i in range(n):
        off, size, name_off = struct.unpack_from('<QQI', meta, 0x10 + i * 0x18)
        name = meta[strtab + name_off:meta.index(b'\0', strtab + name_off)].decode()
        with open(os.path.join(outdir, name), 'wb') as f:
            f.write(view.read(base + 0x10 + n * 0x18 + strsz + off, size))
        names.append((name, size))
    return names


def extract_romfs(view, base, outdir, progress=True):
    h = struct.unpack_from('<10Q', view.read(base, 0x50))
    hsize, _dh_off, _dh_size, dm_off, dm_size, _fh_off, _fh_size, fm_off, fm_size, data_off = h
    if hsize != 0x50:
        raise SystemExit(f'bad RomFS header size 0x{hsize:x}')
    dirs = view.read(base + dm_off, dm_size)
    files = view.read(base + fm_off, fm_size)
    count = total = 0

    def name_of(tbl, o, hdr_len):
        ln = struct.unpack_from('<I', tbl, o + hdr_len - 4)[0]
        return tbl[o + hdr_len:o + hdr_len + ln].decode('utf-8')

    def walk_dir(o, path):
        nonlocal count, total
        _parent, _sib, child, first_file, _hash = struct.unpack_from('<IIIII', dirs, o)
        os.makedirs(path, exist_ok=True)
        f = first_file
        while f != 0xFFFFFFFF:
            _p, sib, doff, dsize, _hh = struct.unpack_from('<IIQQI', files, f)
            name = name_of(files, f, 0x20)
            with open(os.path.join(path, name), 'wb') as out:
                pos, left = base + data_off + doff, dsize
                while left:
                    chunk = view.read(pos, min(left, 16 << 20))
                    out.write(chunk)
                    pos += len(chunk)
                    left -= len(chunk)
            count += 1
            total += dsize
            if progress and count % 500 == 0:
                print(f'  romfs: {count} files, {total / 2**30:.2f} GiB', file=sys.stderr, flush=True)
            f = sib
        d = child
        while d != 0xFFFFFFFF:
            sib = struct.unpack_from('<I', dirs, d + 4)[0]
            walk_dir(d, os.path.join(path, name_of(dirs, d, 0x18)))
            d = sib

    walk_dir(0, outdir)
    return count, total


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('keys')
    ap.add_argument('nca')
    ap.add_argument('outdir')
    ap.add_argument('--no-romfs', action='store_true')
    ap.add_argument('--info', action='store_true')
    a = ap.parse_args()
    nca = Nca(a.nca, load_keys(a.keys))
    print(f'title {nca.title_id:016x}  content type {nca.content_type}  sdk {nca.sdk >> 24}.{(nca.sdk >> 16) & 0xFF}.'
          f'{(nca.sdk >> 8) & 0xFF}  key generation {nca.keygen}')
    for s in nca.sections:
        if s:
            print(f'  section {s.idx}: fs {"PFS0" if s.fs_type == 1 else "RomFS"}  hash {s.hash_type}  enc {s.enc_type}  '
                  f'size 0x{s.size:x}  compressed {bool(s.comp_size)}')
    if a.info:
        return
    if nca.content_type != 0:
        raise SystemExit('not a program NCA')
    exefs = nca.sections[0]
    for name, size in extract_pfs0(View(exefs), 0, os.path.join(a.outdir, 'exefs')):
        print(f'  exefs/{name}  {size}')
    if not a.no_romfs:
        rom = nca.sections[1]
        v = View(rom)
        if v.comp:
            print(f'  romfs compression table: {len(v.comp.entries)} entries')
        n, total = extract_romfs(v, 0, os.path.join(a.outdir, 'romfs'))
        print(f'  romfs: {n} files, {total} bytes')


if __name__ == '__main__':
    main()
