#!/usr/bin/env python3
"""Print a vtable: vtable.py <ghidra addr of first slot (vptr value)> [count]. Slot offsets are bytes."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfread import u32
names = {}
for line in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "../out/functions.tsv")):
    a, _, n = line.rstrip("\n").split("\t")[:3]
    names[int(a, 16)] = n
base = int(sys.argv[1], 16); n = int(sys.argv[2]) if len(sys.argv) > 2 else 64
for i in range(n):
    v = u32(base + 4 * i)
    if v == 0 or v > 0x700000: break
    g = (v & ~1) + 0x10000
    print(f"+0x{4*i:03x}  {g:08x}  {names.get(g, '?')}")
