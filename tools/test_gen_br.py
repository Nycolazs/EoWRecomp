#!/usr/bin/env python3
"""computed_target(): which `br xN` get the all-instruction fallback (address arithmetic or an indexed table load)
and which stay a plain dispatch (callback/vtable loads). Stubbed words, no NSO needed."""
import sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from unittest import mock
for m in ('unicorn', 'unicorn.arm64_const', 'capstone', 'capstone.arm64', 'lz4', 'lz4.block'):  # only computed_target() runs here
    try:
        __import__(m)
    except ImportError:
        sys.modules[m] = mock.MagicMock()
import gen


class K:
    def __init__(self, kind): self.kind = kind


class Stub:
    def __init__(self, words): self.words = words
    def word(self, a): return self.words[a // 4]
    def lift(self, a): return K('op')


BR_X10 = 0xD61F0140
def run(words): return gen.Module.computed_target(Stub(words + [BR_X10]), len(words) * 4, 0)

fail = 0
for name, w, want in (
    ('add x10,x9,x8,lsl#2', 0x8B08292A, True),
    ('adr x10,#0', 0x1000000A, True),
    ('ldr x10,[x9,x8,lsl#3]', 0xF868792A, True),
    ('ldr x10,[x9,#0x10]', 0xF940092A, False),
):
    got = run([w])
    print(('ok   ' if got == want else 'FAIL ') + name)
    fail += got != want
sys.exit(1 if fail else 0)
