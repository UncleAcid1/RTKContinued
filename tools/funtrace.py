#!/usr/bin/env python3
"""Trace the unnamed functions (FUN_xxxxxxxx) of out/decomp.c to the code that owns them.

usage: funtrace.py [--all]   (prints `addr  owner  how  evidence`; --all includes library code)

The 5.11 library exports its C++ methods, so the only unnamed functions are file-static helpers,
callbacks, Ghidra fragments of split functions and library internals. Each one is attributed, in
this order of evidence:
  call     called (or its address taken) by named code of one owner;
  vtable   a slot of an exported `vtable for X`;
  data     stored in a data table (callback table, init_array ...) that named code of one owner reads;
  static-init  listed in .init_array (Ghidra names some _INIT_n): the static initialiser of the
           compilation unit before it (file statics, pools, atexit destructors);
  near     no single owner from references: the named functions around it (same compilation unit)
           belong to one owner;
  none     still ambiguous (owner `?`).
Evidence from FUN_ callers is propagated until nothing changes. Owners are top-level classes, a
library name (lib:SDL ...) or, for free game functions, `free:<name>`.
Kinds: `callback` (a callback-object factory: operator new + vtable, done implicitly by the port's
callback objects) and `static-init`. Used by tools/inventory.py; addresses are Ghidra addresses (file vaddr + 0x10000).
"""
import bisect, os, re, struct, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfread import SO, b, u32

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

# Library code linked into libkingdom.so (free functions by prefix; classes by name).
LIB_PREFIX = re.compile(
    r"^(SDL|GLES2_|SW_|png|FT_|ft_|af_|ps_|TT_|tt_|cff|t1|pfr|psh|Curl|curl|Mix|MIX_|music_|open_music|"
    r"close_music|jpeg|jinit|jcopy|jdiv|jround|jzero|vorbis|ov_|ogg|OGG|res[0-2]_|floor[01]_|mdct|drft|"
    r"Java|JNI_|TTF|IMG|Android|iOS_|inflate|deflate|adler32|crc32|get_crc_table|zlib|zError|WAV|"
    r"UTF8_|restore_core_regs|operator\.(new|delete)|_Unwind|__aeabi|__gnu|getCurrentEnv|"
    r"CallStatic|ConvertParamsToAndroid|channelFinished|post_read_callback|progress_callback|"
    r"write_to_buff|write_to_string|header_info|DisposeBuffer|InitBuffer|base64_|hex2char|char2hex|"
    r"urlencode|urldecode|UrlDecode|UrlEncode|_)")
TEMPLATE = re.compile(r"^(std::|sgl::|HashMap|ObjectBlockPool|DataBlockPool|ChunkedStackPool|GUI::Callback|"
                      r"GUI::ClassCallback|Cache<|rapidjson|__gnu|operator)")
LIB_CLASS = re.compile(r"^(pugi|__cxxabiv1|_JNIEnv|std|rapidjson|sgl|__gnu_cxx|_STL)$")


def owner_of_name(name):
    """Top-level owner of a named function: class, lib:<x>, free:<name>; None for FUN_/_INIT_."""
    if name.startswith(("FUN_", "thunk_FUN_", "_INIT_", "entry")):
        return None
    if "::" in name:
        cls = re.sub(r"<.*", "", name.split("::")[0])
        return "lib:" + cls if LIB_CLASS.match(cls) else cls
    m = LIB_PREFIX.match(name)
    if m:
        return "lib:" + re.sub(r"[_.].*", "", m.group(1)) if m.group(1) not in ("SDL",) else "lib:SDL"
    return "free:" + name


def load_functions():
    """[(addr, name, body)] in address order from out/decomp.c."""
    funcs, cur, body = [], None, []
    for line in open(os.path.join(ROOT, "out/decomp.c"), errors="ignore"):
        if line.startswith("// ==== "):
            if cur:
                funcs.append((cur[0], cur[1], "".join(body)))
            _, _, a, n = line.split(None, 3)
            cur, body = (int(a, 16), n.strip()), []
        else:
            body.append(line)
    if cur:
        funcs.append((cur[0], cur[1], "".join(body)))
    return funcs


def pic_refs(body):
    """Addresses a decompiled body computes PC-relatively (Ghidra's `DAT_lit + 0xoff`)."""
    out = set()
    for lit, off in re.findall(r"DAT_([0-9a-f]{8}) \+ 0x([0-9a-f]+)", body):
        try:
            out.add((u32(int(lit, 16)) + int(off, 16)) & 0xffffffff)
        except Exception:
            pass
    for off, lit in re.findall(r"\(int\)&DAT_([0-9a-f]{8}) \+ DAT_([0-9a-f]{8})", body):
        try:
            out.add((u32(int(lit, 16)) + int(off, 16)) & 0xffffffff)
        except Exception:
            pass
    return out


def data_objects():
    """Exported data symbols [(start, size, name)] (Ghidra addresses), sorted."""
    out = subprocess.run(["objdump", "-T", "--demangle", SO], capture_output=True, text=True).stdout
    objs = []
    for line in out.splitlines():
        m = re.match(r"([0-9a-f]{8}) .*?DO\s+\S+\s+([0-9a-f]{8})\s+(.+)$", line)
        if m:
            objs.append((int(m.group(1), 16) + 0x10000, int(m.group(2), 16), m.group(3).strip()))
    objs.sort()
    return objs


def data_sections():
    shoff = struct.unpack_from("<I", b, 0x20)[0]
    shnum, shstr = struct.unpack_from("<HH", b, 0x30)
    sec = lambda i: struct.unpack_from("<10I", b, shoff + 40 * i)
    stroff = sec(shstr)[4]
    out = []
    for i in range(shnum):
        s = sec(i)
        n = b[stroff + s[0]:b.index(b"\0", stroff + s[0])].decode()
        if n in (".init_array", ".data.rel.ro", ".data", ".got", ".rodata"):
            out.append((n, s[3], s[4], s[5]))   # name, vaddr, file offset, size
    return out


def trace(funcs=None):
    """{addr: (owner, how, evidence, kind)} for every FUN_/thunk_FUN_/_INIT_ function."""
    funcs = funcs or load_functions()
    addrs = [f[0] for f in funcs]
    byaddr = {f[0]: f for f in funcs}
    named = {a: owner_of_name(n) for a, n, _ in funcs}
    unnamed = [a for a, n, _ in funcs if named[a] is None]
    unnamed_set = set(unnamed)

    # refs[target] = set of referencing function addresses
    refs = {a: set() for a in unnamed}
    picref = {}
    for a, n, body in funcs:
        for t in re.findall(r"FUN_([0-9a-f]{8})", body):
            t = int(t, 16)
            if t in refs and t != a:
                refs[t].add(a)
        p = pic_refs(body)
        picref[a] = p
        for t in p:
            if t in refs and t != a:
                refs[t].add(a)

    # Data words pointing at an unnamed function.
    objs = data_objects()
    okeys = [o[0] for o in objs]
    vaddr = {a - 0x10000: a for a in unnamed}
    datahits = {}   # func -> [(ghidra data addr, section)]
    for sname, va, off, size in data_sections():
        for i in range(0, size - 3, 4):
            w = struct.unpack_from("<I", b, off + i)[0] & ~1
            if w in vaddr:
                datahits.setdefault(vaddr[w], []).append((va + i + 0x10000, sname))
    # Who reads a data address: the PIC refs that land at or just below it (table start).
    readers = {}
    for a, p in picref.items():
        for t in p:
            readers.setdefault(t, set()).add(a)
    rkeys = sorted(readers)

    def table_readers(d):
        i = bisect.bisect_right(rkeys, d) - 1
        if i >= 0 and d - rkeys[i] <= 0x400:
            return readers[rkeys[i]], rkeys[i]
        return set(), None

    # Neighbours that tell the compilation unit: named code, not template instances (containers,
    # pools, callbacks are emitted into whichever unit uses them).
    real = {a for a, n, _ in funcs if named[a] and "<" not in n and not TEMPLATE.match(n)}

    def neighbours(a):
        i = bisect.bisect_left(addrs, a)
        prev = next(((x, named[x]) for x in reversed(addrs[:i]) if x in real), None)
        nxt = next(((x, named[x]) for x in addrs[i + 1:] if x in real), None)
        return prev, nxt

    in_init = {f for f, hits in datahits.items() if any(s == ".init_array" for _, s in hits)}

    result = {}
    changed, rounds = True, 0
    while changed and rounds < 20:
        changed, rounds = False, rounds + 1
        for a in unnamed:
            if a in result and result[a][0] != "?":
                continue
            owners, ev = set(), []
            for r in refs[a]:
                o = named[r] or (result.get(r, ("?",))[0])
                if o != "?":
                    owners.add(o)
                    ev.append(byaddr[r][1])
            how = "call"
            if not owners:
                for d, sname in datahits.get(a, []):
                    j = bisect.bisect_right(okeys, d) - 1
                    if j >= 0 and d < objs[j][0] + objs[j][1]:
                        nm = objs[j][2]
                        if nm.startswith("vtable for "):
                            c = nm[len("vtable for "):]
                            owners.add(owner_of_name(c + "::x") or c)
                            ev.append(nm)
                            how = "vtable"
                            continue
                    rs, start = table_readers(d)
                    for r in rs:
                        o = named[r] or result.get(r, ("?",))[0]
                        if o != "?":
                            owners.add(o)
                            ev.append(f"{sname}@{d:x} read by {byaddr[r][1]}")
                            how = "data"
                    if sname == ".init_array":
                        ev.append("init_array")
            owners.discard(None)
            if len(owners) == 1:
                new = (owners.pop(), how, ", ".join(sorted(set(ev)))[:120])
            elif a in in_init or byaddr[a][1].startswith("_INIT_"):
                # A unit's static initialiser (globals, pools, atexit destructors) follows its code.
                prev, _ = neighbours(a)
                new = (prev[1] if prev else "?", "static-init", "init_array; after " +
                       (byaddr[prev[0]][1] if prev else "-"))
            else:
                prev, nxt = neighbours(a)
                po, no = prev and prev[1], nxt and nxt[1]
                if po and no and po.startswith("lib:") and no.startswith("lib:"):
                    new = (po, "near", f"library code: between {byaddr[prev[0]][1]} and {byaddr[nxt[0]][1]}")
                elif po and po == no:
                    new = (po, "near", f"between {byaddr[prev[0]][1]} and {byaddr[nxt[0]][1]}")
                elif owners:
                    game = {o for o in owners if not o.startswith("lib:")}
                    if po in owners or no in owners:
                        o = po if po in owners else no
                        new = (o, "call+near", ", ".join(sorted(owners))[:120])
                    else:
                        new = ("?", "call", "several: " + ", ".join(sorted(owners))[:120])
                    if not game and new[0] == "?":
                        new = (sorted(owners)[0], "call", ", ".join(sorted(owners))[:120])
                else:
                    pn = byaddr[prev[0]][1] if prev else "-"
                    nn = byaddr[nxt[0]][1] if nxt else "-"
                    new = ("?", "none", f"between {pn} and {nn}")
            if result.get(a) != new:
                result[a] = new
                changed = changed or new[0] != "?"
    # Kind tags for the plan: callback-object factories (`new GUI::ClassCallback...(obj, method)`:
    # operator_new + vtable store, nothing else), and static initialisers.
    for a, (o, how, ev) in list(result.items()):
        body = byaddr[a][2]
        code = body[body.find("{"):]
        kind = ""
        if how == "static-init":
            kind = "static-init"
        elif re.search(r"operator_new\(0x[0-9a-f]+\)", code) and re.search(r"= \w+ \+ 8;", code) \
                and len(re.findall(r"\b[A-Za-z_:]+\(", code)) <= 2 and len(code) < 900:
            kind = "callback"
        result[a] = (o, how, ev, kind)
    return result


if __name__ == "__main__":
    funcs = load_functions()
    names = {a: n for a, n, _ in funcs}
    res = trace(funcs)
    for a in sorted(res):
        o, how, ev, kind = res[a]
        if "--all" not in sys.argv and o.startswith("lib:"):
            continue
        print(f"{a:08x}  {names[a]:<22} {o:<28} {how:<11} {kind:<11} {ev}")
