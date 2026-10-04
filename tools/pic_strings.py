#!/usr/bin/env python3
"""Annotate a DumpAsm listing with PIC string literals (ldr rN,=off ; add rM,pc,rN)."""
import re, sys
SO = sys.argv[2] if len(sys.argv) > 2 else __import__("os").path.join(__import__("os").path.dirname(__import__("os").path.abspath(__file__)), "../../Android files/rule-the-kingdom-5-11-multi-android/lib/armeabi-v7a/libkingdom.so")
BASE = 0x10000  # Ghidra image base
blob = open(SO, "rb").read()
regs = {}
for line in open(sys.argv[1]):
    line = line.rstrip("\n")
    m = re.match(r"([0-9a-f]{8})\s+ldr (r\d+),\[0x[0-9a-f]+\].*= 0x([0-9a-f]{8})", line)
    if m:
        regs[m.group(2)] = int(m.group(3), 16)
    m2 = re.match(r"([0-9a-f]{8})\s+add (r\d+),pc,(r\d+)", line)
    if m2 and m2.group(3) in regs:
        a = (regs[m2.group(3)] + int(m2.group(1), 16) + 8) & 0xffffffff
        off = a - BASE
        if 0 <= off < len(blob):
            s = blob[off:blob.find(b"\0", off)][:80]
            if s and all(32 <= c < 127 for c in s):
                line += f'    ; "{s.decode()}"'
    print(line)
