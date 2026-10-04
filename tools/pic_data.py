#!/usr/bin/env python3
"""Annotate a function's disassembly with the values of PIC data reads.

usage: pic_data.py 'Class::Method$'   (reads out/asm_all.txt)

Tracks `ldr rN,[lit] (= value)` followed by `add rN,pc,rN` (rN := pc+8+value, a data address) and
`ldr rN,[pc,rM]` GOT loads (rN := pointer value), then annotates `ldr/str rX,[rN,#off]` and
`vldr sX,[rN,#off]` with the address and its current 32-bit contents (int and float).
Register state is reset at branch targets only approximately (on every label/branch), so check
surprising values against the plain listing.
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfread import rd

A = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../out/asm_all.txt")
pat = re.compile(sys.argv[1])
lit, base = {}, {}
on = False
for line in open(A):
    line = line.rstrip("\n")
    if line.startswith("==== "):
        on = bool(pat.search(line.split()[2])); lit.clear(); base.clear()
        if on: print(line)
        continue
    if not on: continue
    m = re.match(r"([0-9a-f]{8})\s+(\S+)\s*(.*?)(\s+;.*)?$", line)
    if not m: print(line); continue
    addr, op, args = int(m.group(1), 16), m.group(2), m.group(3)
    note = ""
    mm = re.match(r"(r\d+),\[0x[0-9a-f]+\]", args)
    if op == "ldr" and mm and "= 0x" in line:
        lit[mm.group(1)] = int(re.search(r"= 0x([0-9a-f]{8})", line).group(1), 16)
        base.pop(mm.group(1), None)
    elif op in ("add", "addne", "addeq") and re.match(r"(r\d+),pc,(r\d+)$", args):
        d, s = re.match(r"(r\d+),pc,(r\d+)$", args).groups()
        if s in lit: base[d] = (addr + 8 + lit[s]) & 0xffffffff
    elif op.startswith("ldr") and re.match(r"(r\d+),\[pc,(r\d+)\]$", args):
        d, s = re.match(r"(r\d+),\[pc,(r\d+)\]$", args).groups()
        got = re.search(r"\] = 0x([0-9a-f]{8})", line)
        if got: base[d] = int(got.group(1), 16)
    else:
        mm = re.match(r"(?:(r\d+|s\d+|d\d+)),\[(r\d+),#(-?0x[0-9a-f]+)\]", args)
        if mm and mm.group(2) in base and (op.startswith(("ldr", "str", "vldr", "vstr"))):
            a = base[mm.group(2)] + int(mm.group(3), 16)
            b = rd(a, 4)
            if b:
                v = struct.unpack("<I", b)[0]
                note = f"    ; @{a:08x} = {v:#x} ({struct.unpack('<i', b)[0]}, {struct.unpack('<f', b)[0]:.6g})"
        d = re.match(r"(r\d+),", args)
        if d and op.startswith(("ldr", "mov", "add", "sub", "cpy")) and d.group(1) in base and not note.startswith("    ; @") or (d and op.startswith("ldr") and note):
            # destination overwritten
            if d.group(1) != (mm.group(2) if mm else None) or True:
                base.pop(d.group(1), None); lit.pop(d.group(1), None)
    print(line + note)
