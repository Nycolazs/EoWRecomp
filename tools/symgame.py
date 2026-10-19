"""Guest call stack from a crash log: the backtrace's eowgame.so+<offset> frames -> recompiled function f_<addr>
-> module + symbol (sym.py). Usage: symgame.py <log> [library]"""
import bisect
import re
import subprocess
import sys

import sym

log = open(sys.argv[1], errors='replace').read()
lib = sys.argv[2] if len(sys.argv) > 2 else '../build/eowgame.so'
rows = []
for line in subprocess.run(['nm', lib], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) == 3 and p[2].startswith('f_'):
        rows.append((int(p[0], 16), int(p[2][2:], 16)))
rows.sort()
addrs = [a for a, _ in rows]
for off in re.findall(r'eowgame\.so\+([0-9a-f]+)', log):
    o = int(off, 16)
    i = bisect.bisect_right(addrs, o) - 1
    if i < 0:
        print(f'  eowgame.so+{off}: ?')
        continue
    g = rows[i][1]
    r = sym.lookup(g)
    name = f'{r[0]}+{r[2]:x} {sym.demangle(r[3]) if r[3] else "?"}+{r[4]:x}' if r else '?'
    print(f'  f_{g:x}  {name}')
