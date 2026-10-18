#!/usr/bin/env python3
"""asmfn.py ADDR: one function's asm from out/asm_all.txt (from ADDR to the next ==== header), with
GOT-loaded symbols named. Use it when the decompile shows extraout_*, mixes floats and ints, or a
function decompiled to almost nothing (Ghidra's size limit)."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import picsym
a = sys.argv[1].lower().replace('0x', '').rjust(8, '0')
out, p = [], False
for l in open(os.path.join(HERE, '..', 'out', 'asm_all.txt')):
    if l.startswith('==== '):
        if p: break
        continue
    if l.startswith(a): p = True
    if p:
        m = re.search(r'\[(00[0-9a-f]{6})\] = 0x(00[0-9a-f]{6})', l)
        if m:
            n = picsym.name(int(m.group(2), 16))
            if n and not n.startswith('g_'): l = l.rstrip('\n') + '   <' + n + '>\n'
        out.append(l)
sys.stdout.write(''.join(out))
