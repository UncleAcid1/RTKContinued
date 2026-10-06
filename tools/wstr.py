#!/usr/bin/env python3
"""Print the UTF-32 (Android wchar_t) string at Ghidra address(es): wstr.py 0x58c4a4 ..."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfread import u32
for a in sys.argv[1:]:
    a = int(a, 16); out = []
    while len(out) < 400:
        c = u32(a + 4 * len(out))
        if c == 0: break
        out.append(chr(c) if c < 0x110000 else '?')
    print(hex(a), repr(''.join(out)))
