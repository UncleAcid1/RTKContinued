#!/usr/bin/env python3
"""Second pass after picsym.py: resolve `iVarN + 0x<pc>` where iVarN = DAT_<lit> was assigned
earlier in the same function (Ghidra leaves these when the add-pc is far from the load).

usage: tools/fn.sh 'BuildingPlacement::' | tools/picsym.py | tools/picvar.py
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfread import u32
from picsym import resolve

out, env = [], {}
for line in sys.stdin.read().split("\n"):
    if line.startswith("// ==== "): env = {}
    for v, lit in re.findall(r"(\w+) = DAT_([0-9a-f]{8})[;,)]", line):
        env[v] = int(lit, 16)
    def sub(m):
        v, off = m.group(1), int(m.group(2), 16)
        if v not in env: return m.group(0)
        try: return resolve((u32(env[v]) + off) & 0xffffffff)
        except Exception: return m.group(0)
    line = re.sub(r"\(?(\w+) \+ 0x([0-5][0-9a-f]{5})\)?", sub, line)
    out.append(line)
print("\n".join(out))
