#!/usr/bin/env python3
"""dat.py ADDR...: the 8 bytes at each Ghidra address as int, float and double (DAT_ constants)."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfread import rd
for a in sys.argv[1:]:
    d = rd(int(a, 16), 8)
    i = struct.unpack_from('<I', d)[0]
    print(f"{a}: 0x{i:x} {i} f={struct.unpack_from('<f', d)[0]:g} d={struct.unpack_from('<d', d)[0]:g}")
