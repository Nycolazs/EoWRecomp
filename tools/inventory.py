"""Inventory of the game's ExeFS against what the runtime implements (data for docs/GAP.md).

Usage: inventory.py <exefs dir> [--lift] [--simd c|asm]

Per module: segments, build id, SDK strings, imports by the module that exports them, NVN entry point names, HOS
service names, SVCs; with --lift, every instruction word of .text through the lifter, Unsupported reasons counted.
"""
import argparse
import collections
import os
import re
import struct
import sys

from nso import Nso

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
MODULES = ['main', 'sdk', 'subsdk0', 'subsdk1', 'subsdk2', 'subsdk3']

# HOS service names (Switchbrew "Services API"): looked for as NUL-terminated strings
SERVICES = '''acc:u0 acc:u1 acc:aa am:u appletAE appletOE apm apm:p aoc:u aoc:u2 audin:u audout:u audrec:u audren:u
audctl auddev bcat:u bcat:s bsd:u bsd:s caps:a caps:u caps:su caps:ss csrng erpt:c es fatal:u friend:u friend:a fsp-ldr
fsp-pr fsp-srv hid hid:sys hid:dbg hidbus irs ldn:u lm lr mii:e mii:u mm:u nfc:user nfp:user nifm:u nifm:a nim ns:am2
ns:ec ns:su ns:vm nsd:u nvdrv nvdrv:a nvdrv:s nvmemp ovln:snd pctl pctl:s pctl:r pctl:a pdm:qry pl:u pl:s prepo:u prepo:a
psc:m psm sfdnsres set set:sys set:cal sm: spl: ssl time:u time:a time:s tc vi:u vi:s vi:m wlan:inf ectx:aw ectx:w
grc:c olsc:u ngct:u ngc:u mnpp:app arp:r arp:w btm btm:u btdrv capmtp sprof:bg'''.split()


def implemented():
    rt = os.path.join(ROOT, 'runtime')
    hle, services, nvn = set(), set(), set()
    src = ''
    for d, _, fs in os.walk(rt):
        for f in fs:
            if f.endswith(('.c', '.m', '.mm', '.cpp')):
                src += open(os.path.join(d, f), errors='replace').read()
    hle |= set(re.findall(r'HLE_FUNC\("([^"]+)"', src))
    nvn |= set(re.findall(r'^(?:NVN|PLAIN)\((nvn\w+)', src, re.M))
    services |= set(re.findall(r'SERVICE\("([^"]+)"', src))
    k = open(os.path.join(rt, 'kernel.c')).read()
    body = k[k.rindex('static void svc_impl'):k.index('void kernel_stop_threads')]
    svcs = {int(x, 16) for x in re.findall(r'case (0x[0-9A-Fa-f]+):', body)}
    return hle, services, nvn, svcs


def text_words(n):
    t0, ts = n.text
    return struct.unpack(f'<{ts // 4}I', bytes(n.image[t0:t0 + ts]))


def lift_census(n, base, simd):
    from lifter import Lifter, Unsupported
    lf = Lifter(base, simd)
    reasons = collections.Counter()
    first = {}
    for i, w in enumerate(text_words(n)):
        a = base + n.text[0] + i * 4
        try:
            ins = lf.lift(w, a)
        except Unsupported as e:
            r = re.sub(r'[0-9a-f]{8}', 'XXXXXXXX', str(e))
            reasons[r] += 1
            first.setdefault(r, (a, w))
            continue
        except Exception as e:  # a lifter bug, not an unsupported encoding
            r = f'EXCEPTION {type(e).__name__}: {e}'
            reasons[r] += 1
            first.setdefault(r, (a, w))
    return reasons, first


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exefs')
    ap.add_argument('--lift', action='store_true')
    ap.add_argument('--simd', choices=('c', 'asm'), default='c')
    a = ap.parse_args()
    hle, services_impl, nvn_impl, svc_impl = implemented()
    mods = {m: Nso(os.path.join(a.exefs, m)) for m in MODULES if os.path.exists(os.path.join(a.exefs, m))}
    exports = {m: {s[0]: s for s in n.exports()} for m, n in mods.items()}
    print('## Modules\n')
    print('| module | build id | .text | .rodata | .data+bss | image | syms | imports | exports | SDK strings |')
    print('|---|---|---|---|---|---|---|---|---|---|')
    for m, n in mods.items():
        img = bytes(n.image)
        sdk = sorted(set(x.decode().replace('SDK MW+', '') for x in re.findall(rb'SDK MW\+[ -~]{4,100}', img)))
        print(f'| {m} | `{n.build_id[:16]}` | 0x{n.text[1]:x} | 0x{n.rodata[1]:x} | 0x{n.data[1] + n.bss_size:x} | '
              f'0x{n.size:x} | {len(n.syms)} | {len(n.imports())} | {len(n.exports())} | {", ".join(sdk) or "-"} |')
    print('\n## Imports by exporting module\n')
    for m, n in mods.items():
        by = collections.Counter()
        unres = []
        hle_hit = 0
        for s in n.imports():
            name = s[0]
            src = [k for k in mods if k != m and name in exports[k]]
            by[src[0] if src else 'unresolved'] += 1
            if not src:
                unres.append(name)
            if name in hle:
                hle_hit += 1
        print(f'- **{m}**: {dict(by)}; bound to host HLE: {hle_hit}' +
              (f'; unresolved: {", ".join(unres)}' if unres else ''))
    print('\n## NVN entry points named in the modules\n')
    names = set()
    for m, n in mods.items():
        names |= {x.decode() for x in re.findall(rb'\x00(nvn[A-Z]\w+)\x00', bytes(n.image))}
    special = {'nvnBootstrapLoader', 'nvnDeviceGetProcAddress'}
    missing = sorted(x for x in names if x not in nvn_impl and x not in special)
    print(f'{len(names)} names, {len(names & nvn_impl)} implemented, {len(missing)} missing:\n')
    for x in missing:
        print(f'- `{x}`')
    print('\n## HOS service names present as strings\n')
    found = collections.defaultdict(list)
    for m, n in mods.items():
        img = bytes(n.image)
        for sname in SERVICES:
            if b'\x00' + sname.encode() + b'\x00' in img:
                found[sname].append(m)
    print('| service | in modules | runtime |')
    print('|---|---|---|')
    for sname in sorted(found):
        print(f'| `{sname}` | {", ".join(found[sname])} | {"implemented" if sname in services_impl else "**stub**"} |')
    print('\n## SVCs (svc #imm sites)\n')
    for m, n in mods.items():
        s = collections.Counter()
        for w in text_words(n):
            if (w & 0xFFE0001F) == 0xD4000001:
                s[(w >> 5) & 0xFFFF] += 1
        if s:
            miss = sorted(k for k in s if k not in svc_impl)
            print(f'- **{m}**: {len(s)} distinct, {len(set(s) & svc_impl)} implemented; not implemented: '
                  + ' '.join(f'0x{k:02X}' for k in miss))
    if a.lift:
        print(f'\n## Lifter census (--simd {a.simd})\n')
        bases = {'main': 0x7000000000, 'sdk': 0x7010000000, 'subsdk0': 0x7012000000, 'subsdk1': 0x7014000000}
        for m, n in mods.items():
            reasons, first = lift_census(n, bases.get(m, 0x7020000000), a.simd)
            total = sum(reasons.values())
            print(f'- **{m}**: {len(text_words(n))} words, {total} unsupported')
            for r, k in reasons.most_common():
                fa, fw = first[r]
                print(f'  - {k} × {r} (first at {fa:#x}: {fw:08x})')
            sys.stdout.flush()


if __name__ == '__main__':
    main()
