#!/usr/bin/env python3
"""gotslot.py LIT...: the global behind a GOT slot. LIT is the address of a function's literal
(DAT_xxxxxxxx) holding the slot's offset from the .got start, or the offset itself (0x...)."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import picsym
for arg in sys.argv[1:]:
    v = int(arg, 16)
    off = picsym.u32(v) if v > 0x10000 else v
    t = picsym.u32(picsym.GOT[0] + off)
    print(arg, '->', picsym.name(t + 0x10000 if t else 0), hex(t + 0x10000))
