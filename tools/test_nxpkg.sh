#!/bin/sh
# tools/test_nxpkg.sh [prod.keys package]: builds and runs tools/test_nxpkg.c (LZ4 blocks from the lz4 module in
# tools/venv, of random and repetitive data)
set -e
cd "$(dirname "$0")/.."
O=build/test_nxpkg
mkdir -p $O
${CC:-clang} -O2 -Wall -o $O/test_nxpkg tools/test_nxpkg.c -lpthread
tools/venv/bin/python -I -c "
import lz4.block, os, random, struct, sys
random.seed(1)
cases = {'random': os.urandom(70000), 'zeros': bytes(65536), 'text': b'Echoes of Wisdom ' * 5000,
         'mixed': b''.join(random.choice([os.urandom(37), b'ab' * 300, bytes(1000)]) for _ in range(300))}
for k, raw in cases.items():
    c = lz4.block.compress(raw, store_size=False, mode='high_compression' if k == 'mixed' else 'default')
    open('$O/' + k + '.lz4', 'wb').write(struct.pack('<II', len(raw), len(c)) + raw + c)
"
$O/test_nxpkg --lz4 $O/random.lz4 --lz4 $O/zeros.lz4 --lz4 $O/text.lz4 --lz4 $O/mixed.lz4
[ $# -ge 2 ] && $O/test_nxpkg "$1" "$2"
exit 0
