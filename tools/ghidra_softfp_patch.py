#!/usr/bin/env python3
"""Make Ghidra's ARM default calling convention softfp (floats in r0-r3, float results in r0).

libkingdom.so is built with -mfloat-abi=softfp: float arguments and results travel in core
registers. Ghidra's default ARM prototype is hard-float (s0-s15), so the decompiler loses every
float argument (extraout_sN). This rewrites the <default_proto> of ARM.cspec in place, removing
the float register entries and float rules while keeping d8-d15 callee-saved (VFP is still used
inside functions). A backup is kept as ARM.cspec.orig; run with --restore to undo.
"""
import re, shutil, subprocess, sys
prefix = subprocess.check_output(["brew", "--prefix", "ghidra"], text=True).strip()
path = prefix + "/libexec/Ghidra/Processors/ARM/data/languages/ARM.cspec"
if "--restore" in sys.argv:
    shutil.copy(path + ".orig", path); print("restored", path); sys.exit()
src = open(path + ".orig" if __import__("os").path.exists(path + ".orig") else path).read()
if not __import__("os").path.exists(path + ".orig"):
    shutil.copy(path, path + ".orig")
a = src.index("<default_proto>"); b = src.index("</default_proto>")
proto = src[a:b]
proto = re.sub(r'\s*<pentry[^>]*metatype="float"[^>]*>\s*<register name="[sd]\d+"/>\s*</pentry>', "", proto)
proto = re.sub(r'\s*<!--[^>]*-->', "", proto)
# drop every <rule> whose datatype is float-related (keeps the generic "any"/struct/union rules)
proto = re.sub(r'\s*<rule>\s*<datatype name="(float|homogeneous-float-aggregate)"[^>]*/>.*?</rule>', "", proto, flags=re.S)
assert 'metatype="float"' not in proto and 'name="float"' not in proto
open(path, "w").write(src[:a] + proto + src[b:])
print("patched", path)
