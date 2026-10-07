#!/usr/bin/env python3
"""Port coverage per top-level class: inventory.py [--missing]

Every function in out/decomp.c is grouped by its top-level class (nested classes and templates fold
into their owner; container/pool/callback/JSON templates and library code are left out). A function
counts as ported when its address appears as `@0x...` anywhere in src/. Prints
`ported/total  class`; with --missing only classes with nothing ported. Used to keep
docs/port_inventory.md honest: a class here that the inventory doesn't place is a gap in the plan.
"""
import os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SKIP = re.compile(r"^(sgl::|HashMap|ObjectBlockPool|DataBlockPool|ChunkedStackPool|GUI::Callback|"
                  r"GUI::ClassCallback|rapidjson|std$|std::|vec3|Cache<|__gnu)")

ported = set()
for d, _, files in os.walk(os.path.join(ROOT, "src")):
    for f in files:
        for m in re.finditer(r"@0x([0-9a-fA-F]+)", open(os.path.join(d, f), errors="ignore").read()):
            ported.add(int(m.group(1), 16))

done, total = {}, {}
for line in open(os.path.join(ROOT, "out/decomp.c"), errors="ignore"):
    if not line.startswith("// ==== "):
        continue
    _, _, addr, name = line.split(None, 3)
    name = name.strip()
    if "::" not in name or SKIP.match(name):
        continue
    cls = re.sub(r"<.*", "", name.split("::")[0])
    total[cls] = total.get(cls, 0) + 1
    done[cls] = done.get(cls, 0) + (int(addr, 16) in ported)

for cls in sorted(total):
    if "--missing" in sys.argv and done[cls]:
        continue
    print(f"{done[cls]:4}/{total[cls]:<4} {cls}")
