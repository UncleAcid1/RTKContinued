#!/usr/bin/env python3
"""Resolve PIC global addresses in decompiled C to names, and drop STL template noise.

usage: tools/fn.sh 'GameState::Reset$' | tools/picsym.py

Ghidra folds `ldr rN,[lit]; add rN,pc,rN` into `DAT_<lit> + 0x<pc+8>`; the global is then at
u32(lit) + pc+8. Each such expression is replaced by `[name]` (exported symbol, +offset if inside
one) or `[g_<addr>]`. If the address is a GOT slot, the slot's target is named instead: `[GOT:name]`;
`v + DAT_<lit>` with v = GOTBASE (the .got start) becomes `&[name]` (the slot GOTBASE + u32(lit)).
Addresses are Ghidra addresses (file vaddr + 0x10000), as everywhere in the port's comments.
"""
import bisect, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfread import SO, u32, b, rd

def load_syms():
    out = subprocess.run(["objdump", "-T", "--demangle", SO], capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        m = re.match(r"([0-9a-f]{8})\s.*?\s([0-9a-f]{8})\s+(.+)$", line)
        if not m or m.group(1) == "00000000": continue
        name = re.sub(r"\(.*", "", m.group(3)).strip()
        syms.append((int(m.group(1), 16) + 0x10000, int(m.group(2), 16), name))
    syms.sort()
    return syms

def got_range():
    import struct
    shoff = struct.unpack_from('<I', b, 0x20)[0]; shnum, shstr = struct.unpack_from('<HH', b, 0x30)
    sec = lambda i: struct.unpack_from('<10I', b, shoff + 40 * i)
    stroff = sec(shstr)[4]
    for i in range(shnum):
        s = sec(i); n = b[stroff + s[0]:b.index(b'\0', stroff + s[0])].decode()
        if n == ".got": return s[3] + 0x10000, s[3] + 0x10000 + s[5]
    return 0, 0

SYMS = load_syms(); KEYS = [s[0] for s in SYMS]; GOT = got_range()

def name(a):
    i = bisect.bisect_right(KEYS, a) - 1
    if i >= 0:
        s, sz, n = SYMS[i]
        if a == s: return n
        if a < s + max(sz, 1): return f"{n}+{a - s:#x}"
    return f"g_{a:x}"

def resolve(a):
    if a == GOT[0]: return "GOTBASE"
    if GOT[0] <= a < GOT[1]:
        t = u32(a)
        return f"[GOT:{name(t + 0x10000 if t else 0)}]"
    n = name(a)
    if n.startswith("g_"):   # a string literal?
        d = rd(a, 100)
        if d and b"\0" in d:
            s = d[:d.index(b"\0")]
            if len(s) >= 1 and all(32 <= c < 127 or c in (9, 10) for c in s):
                return '"' + s.decode().replace("\n", "\\n") + '"'
    return f"[{n}]"

def sub_pair(m):
    lit, off = int(m.group(1), 16), int(m.group(2), 16)
    try: return resolve((u32(lit) + off) & 0xffffffff)
    except Exception: return m.group(0)

def sub_amp(m):
    off, lit = int(m.group(1), 16), int(m.group(2), 16)
    try: return resolve((u32(lit) + off) & 0xffffffff)
    except Exception: return m.group(0)

src = sys.stdin.read()
src = re.sub(r"\(int\)&DAT_([0-9a-f]{8}) \+ DAT_([0-9a-f]{8})", sub_amp, src)
src = re.sub(r"DAT_([0-9a-f]{8}) \+ 0x([0-9a-f]+)", sub_pair, src)
# `v = GOTBASE; ... *(T **)(v + DAT_<lit>)`: the GOT slot at GOTBASE + u32(lit) points at a global.
def sub_slot(m):
    try:
        t = u32(GOT[0] + u32(int(m.group(1), 16)))
        return f"&[{name(t + 0x10000)}]"
    except Exception: return m.group(0)
for v in set(re.findall(r"(\w+) = GOTBASE;", src)):
    src = re.sub(rf"\b{v} \+ DAT_([0-9a-f]{{8}})", sub_slot, src)
# Collapse STLport container template spellings.
prev = None
while prev != src:
    prev = src
    src = re.sub(r"(_Rb_tree|vector|_Select1st|_MapTraitsT|_SetTraitsT|allocator|pair|less|_Identity|_String_base|basic_string|_List_base|list)<[^<>]*>", r"\1<>", src)
sys.stdout.write(src)
