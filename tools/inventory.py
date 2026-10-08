#!/usr/bin/env python3
"""Port coverage: inventory.py [--missing | --functions [STEP] | --unplanned | --steps | --trace |
                               --holes | --unmarked]

Every game function in out/decomp.c is grouped by its top-level class (nested classes and templates
fold into their owner; container/pool/callback/JSON templates and library code are left out).
Unnamed functions (FUN_xxxxxxxx, static initialisers) are traced to their owner by
tools/funtrace.py (callers, vtables, data tables, init_array, neighbouring code) and counted with
it. A function counts as ported when its address appears as `@0x...` anywhere in src/ (or, for an
unnamed one, `FUN_xxxxxxxx` is named in a src/ comment: read and handled there).

  (no flag)        `ported/total  class` per owner (classes, and `free:` for free game functions)
  --missing        only owners with nothing ported
  --functions [S]  every unported game function with its step (only step S if given)
  --unplanned      unported functions no rule in tools/port_plan.txt places: gaps in the plan
  --steps          unported function count per step
  --trace          the unnamed functions with their owner and the evidence
  --holes          unported functions that ported code calls, with their step and callers
  --unmarked       unported functions whose name src/ defines in the same class or namespace: likely
                   ported without their `// @0x` comment (check each, then add the comment)

The plan is tools/port_plan.txt: `step  regex  # note`, first match wins, matched against
`Class::Method` or a free function's name (a rule `step  @xxxxxxxx` places the one function at that
address, for an overload that goes elsewhere than its siblings) (an unnamed function is `Owner::FUN_xxxxxxxx`, with ` (callback)` or ` (static-init)`
appended for those kinds). Used to keep docs/port_inventory.md
and STATUS.md honest: anything --unplanned prints is a gap to place.
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funtrace

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SKIP = re.compile(r"^(sgl::|HashMap|ObjectBlockPool|DataBlockPool|ChunkedStackPool|GUI::Callback|"
                  r"GUI::ClassCallback|rapidjson|std$|std::|vec3|Cache<|__gnu|operator)")
STEP_ORDER = ["4b.4", "4b.5", "4c", "4d", "4e", "4f", "4g", "4h", "5", "6", "7", "online", "removed", "dead", "implicit", "util",
              "lib"]


def ported_addrs():
    out = set()
    for d, _, files in os.walk(os.path.join(ROOT, "src")):
        for f in files:
            src = open(os.path.join(d, f), errors="ignore").read()
            # `@0x...` marks a port; a `FUN_xxxxxxxx` named in a comment was read and handled there
            # (ported under a port name, or recorded as having no effect).
            for m in re.finditer(r"@0x([0-9a-fA-F]+)|FUN_([0-9a-fA-F]{8})", src):
                out.add(int(m.group(1) or m.group(2), 16))
    return out


def src_definitions():
    """{(scope, name)} of function definitions in src/: `Scope::Name(` at the start of a line, or
    `Name(` at the start of a line in a file that opens `namespace Scope` / is named Scope.*."""
    defs = set()
    for d, _, files in os.walk(os.path.join(ROOT, "src")):
        for f in files:
            if not f.endswith(".cpp"):
                continue
            src = open(os.path.join(d, f), errors="ignore").read()
            scopes = set(re.findall(r"^namespace (\w+)", src, re.M)) | {f.split(".")[0]}
            for m in re.finditer(r"^(?:[\w:<>,*& ]+[ *&])?((?:\w+::)*)(~?\w+)\([^;{}]*\)\s*(?:const\s*)?\{",
                                 src, re.M):
                qual, name = m.group(1), m.group(2)
                if qual:
                    parts = qual.rstrip(":").split("::")
                    defs.add((parts[0], name))
                    defs.add((parts[-1], name))
                else:
                    for sc in scopes:
                        defs.add((sc, name))
    return defs


def load_plan():
    """Rules from tools/port_plan.txt: `step  regex  [# note]` (regexes have no spaces)."""
    rules = []
    for n, line in enumerate(open(os.path.join(ROOT, "tools/port_plan.txt")), 1):
        body, _, note = line.partition("#")
        parts = body.split()
        if not parts:
            continue
        if len(parts) != 2:
            sys.exit(f"port_plan.txt:{n}: expected `step  regex  [# note]`")
        rx = parts[1]
        if rx.startswith("@"):   # one function by address (Ghidra address, hex)
            rx = "^@%08x$" % int(rx[1:], 16)
        rules.append((re.compile(rx), parts[0], note.strip(), n))
    return rules


def game_functions():
    """[(addr, owner, key, name)] for every game function; key = Owner::Name for the plan."""
    funcs = funtrace.load_functions()
    traced = funtrace.trace(funcs)
    out = []
    for a, name, _ in funcs:
        if name.startswith(("FUN_", "thunk_FUN_", "_INIT_")) or name == "entry":
            owner, how, _, kind = traced.get(a, ("?", "none", "", ""))
            if owner.startswith("lib:"):
                continue
            key = f"{owner}::{name}" + (f" ({kind})" if kind else "")
        else:
            if SKIP.match(name):
                continue
            owner = funtrace.owner_of_name(name)
            if owner is None or owner.startswith("lib:"):
                continue
            key = name   # Class::Method, or a free function's bare name
        out.append((a, owner, key, name))
    return out, traced


def plan_step(rules, key, addr=None):
    if addr is not None:
        for rx, step, note, n in rules:
            if rx.pattern.startswith("^@") and rx.search("@%08x" % addr):
                return step, note, n
    prev = None
    while prev != key:   # template arguments out: EventManager<Entity>::X -> EventManager::X
        prev, key = key, re.sub(r"<[^<>]*>", "", key)
    for rx, step, note, n in rules:
        if rx.search(key):
            return step, note, n
    return None, "", 0


def main():
    args = sys.argv[1:]
    ported = ported_addrs()
    funcs, traced = game_functions()
    if "--trace" in args:
        names = {a: n for a, _, _, n in funcs}
        for a in sorted(traced):
            o, how, ev, kind = traced[a]
            if o.startswith("lib:"):
                continue
            print(f"{a:08x}  {names.get(a, '?'):<14} {o:<26} {how:<11} {kind:<11} {ev}")
        return
    if "--holes" in args:
        # Unported functions that ported code calls (by name, or FUN_ address): holes in finished
        # work, stubbed or inlined by the port. Each needs a step or a recorded reason.
        bodies = {a: body for a, _, body in funtrace.load_functions()}
        byname = {}
        for a, owner, key, name in funcs:
            if a not in ported:
                byname.setdefault(name, []).append(a)
        keyof = {a: key for a, _, key, _ in funcs}
        callers = {}
        for a, owner, key, name in funcs:
            if a not in ported:
                continue
            body = bodies[a]
            for t in set(re.findall(r"([A-Za-z_][\w:~]*(?:<[^()<>]*>)?)\(", body)):
                for u in byname.get(t, []):
                    if u != a:
                        callers.setdefault(u, set()).add(name)
        rules = load_plan()
        for u in sorted(callers):
            step = plan_step(rules, keyof[u], u)[0] or "NO PLAN"
            cs = sorted(callers[u])
            print(f"{u:08x}  {step:<8} {keyof[u]}  <- {', '.join(cs[:4])}" +
                  (f" (+{len(cs) - 4})" if len(cs) > 4 else ""))
        return
    if "--unmarked" in args:
        defs = src_definitions()
        for a, owner, key, name in funcs:
            if a in ported or "::" not in name or "FUN_" in name:
                continue
            parts = name.split("::")
            meth = parts[-1]
            if meth.startswith("~") or meth == parts[-2]:
                continue
            if (parts[-2], meth) in defs or (owner, meth) in defs:
                print(f"{a:08x}  {name}")
        return
    if "--functions" in args or "--unplanned" in args or "--steps" in args:
        rules = load_plan()
        want = None
        if "--functions" in args:
            i = args.index("--functions")
            want = args[i + 1] if i + 1 < len(args) else None
        counts, used = {}, set()
        for a, owner, key, name in funcs:
            step, note, n = plan_step(rules, key, a)
            used.add(n)
            if a in ported:
                continue
            step = step or "NO PLAN"
            counts[step] = counts.get(step, 0) + 1
            if "--unplanned" in args and step != "NO PLAN":
                continue
            if "--functions" in args and want and step != want:
                continue
            if "--steps" in args:
                continue
            print(f"{a:08x}  {step:<8} {key}" + (f"   [{note}]" if note else ""))
        if "--steps" in args:
            order = {s: i for i, s in enumerate(STEP_ORDER)}
            for s in sorted(counts, key=lambda s: (order.get(s, -1), s)):
                print(f"{counts[s]:5}  {s}")
            dead = [n for rx, _, _, n in rules if n not in used]
            if dead:
                print("rules matching nothing (port_plan.txt lines):", dead)
        return
    done, total = {}, {}
    for a, owner, key, name in funcs:
        total[owner] = total.get(owner, 0) + 1
        done[owner] = done.get(owner, 0) + (a in ported)
    for cls in sorted(total):
        if "--missing" in args and done[cls]:
            continue
        print(f"{done[cls]:4}/{total[cls]:<4} {cls}")


if __name__ == "__main__":
    main()
