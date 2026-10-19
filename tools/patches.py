"""Per-instruction replacements applied by gen.py (guest address -> function(gen, addr) returning C code).

Every patch checks the original instruction word at its address before replacing it (expect()), so a wrong
address - or a different game build - fails generation loudly instead of miscompiling silently. Addresses are
absolute guest addresses (module base + offset) and valid only for the pinned build of the game (docs/STATUS.md).

Example:
    SITE = 0x7000123456

    def _site(gen, a):
        expect(gen, a, 0xbd400000)  # ldr s0, [x0]
        return '{ ... C replacing the instruction ... }'

    PATCHES[SITE] = _site
"""
import struct

PATCHES = {}


def word_at(gen, a):
    return struct.unpack_from('<I', gen.nso.image, a - gen.base)[0]


def expect(gen, a, want, mask=0xFFFFFFFF):
    """Assert the original instruction at guest address a (under mask) before patching it."""
    w = word_at(gen, a)
    assert (w & mask) == want, f'patch site {a:#x}: found {w:08x}, expected {want:08x} (mask {mask:08x})'
    return w
