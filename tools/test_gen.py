"""Unit tests for tools/gen.py helpers that need no game files (synthesized encodings).

Usage: tools/venv/bin/python -I tools/test_gen.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen import adr_feeds_br  # noqa: E402


class M:
    def __init__(self, words):
        self.words = words


ADR_X10 = 0x1000010A      # adr x10, #+0x20
ADR_X1 = 0x10000101       # adr x1, #+0x20
LDRH = 0x7868792B         # ldrh w11, [x9, x8, lsl #1]
ADD = 0x8B0B094A          # add x10, x10, x11, lsl #2
MOV = 0x2A0203F6          # mov w22, w2
MOV_X10 = 0xAA0003EA      # mov x10, x0
BR_X10 = 0xD61F0140       # br x10
BR_X1 = 0xD61F0020        # br x1
BL = 0x94000010           # bl #+0x40
BCOND = 0x54000080        # b.eq #+0x10

CASES = [
    ('table, br right after', [ADR_X10, LDRH, ADD, BR_X10], True),
    # main 7001a657e0: four moves scheduled between the add and the br (7 instructions: past the old window of 6)
    ('table, br 7 instructions on', [ADR_X10, LDRH, ADD, MOV, MOV, MOV, MOV, BR_X10], True),
    # main 70018100d0: 17 instructions (FP math) between them
    ('table, br 17 instructions on', [ADR_X10, LDRH, ADD] + [MOV] * 13 + [BR_X10], True),
    ('callback passed to a call', [ADR_X1, BL, BR_X1], False),
    ('conditional branch first', [ADR_X10, LDRH, ADD, BCOND, BR_X10], False),
    ('register overwritten', [ADR_X10, MOV_X10, BR_X10], False),
    ('br on another register', [ADR_X10, LDRH, ADD, BR_X1], False),
]


def main():
    fails = 0
    for name, words, want in CASES:
        got = adr_feeds_br(M(words), 0, words[0] & 31)
        if got != want:
            print(f'FAIL adr_feeds_br: {name}: {got}, want {want}')
            fails += 1
    print(f'{len(CASES) - fails}/{len(CASES)} pass')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
