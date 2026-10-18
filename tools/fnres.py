#!/usr/bin/env python3
"""fnres.py NAME [DECOMP]: one function of out/decomp.c (by its full name, e.g. HireTroopsWindow::Show)
with `iVarN + 0x...` (iVarN = DAT_x loaded earlier) resolved to g_<addr> and declaration noise dropped.
For named globals and GOT slots pipe tools/fn.sh through tools/picsym.py | tools/picvar.py instead."""
import os, re, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from elfread import u32, rd
src = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, '..', 'out', 'decomp.c')
name = sys.argv[1]
out, p = [], False
for line in open(src):
    if line.startswith('// ==== '):
        if p: break
        parts = line.split()
        p = len(parts) > 3 and parts[3] == name
    if p: out.append(line)
var, res = {}, []
for l in out:
    m = re.search(r'(iVar\d+) = (.*?);', l)
    if m:   # the variable now holds DAT_x's word (only while that is readable)
        d = re.fullmatch(r'DAT_([0-9a-f]{8})', m.group(2))
        if d and rd(int(d.group(1), 16), 4): var[m.group(1)] = int(d.group(1), 16)
        else: var.pop(m.group(1), None)
    def sub(mm):
        v, o = mm.group(1), int(mm.group(2), 16)
        return 'g_%x' % ((u32(var[v]) + o) & 0xffffffff) if v in var else mm.group(0)
    l = re.sub(r'(iVar\d+) \+ 0x([0-9a-f]+)', sub, l)
    l = re.sub(r'\(int\)&DAT_([0-9a-f]{8}) \+ (iVar\d+)',
               lambda mm: 'g_%x' % ((u32(var[mm.group(2)]) + int(mm.group(1), 16)) & 0xffffffff)
               if mm.group(2) in var else mm.group(0), l)
    if re.match(r'^  [a-zA-Z_]+[ *]+[a-zA-Z0-9_]+( \[[0-9a-fx]+\])?;$', l): continue
    if re.match(r'^ *(local|uStack|puStack|auStack|ppp)\w* = (0|&|uStack|local|\(undefined)', l): continue
    if l.strip() == '': continue
    res.append(l)
sys.stdout.write(''.join(res))
