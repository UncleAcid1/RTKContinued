#!/usr/bin/env python3
"""Rule the Kingdom .xmlb (compact Flash-exported UI layout) parser.

Transcribed from GUI::RegisterBinaryUI (libkingdom.so 5.11, 0x180784).
File = 4 sections, each [u32 count][records]:
  windows  26 B  CompactWindowInfo (13 x u16, see WINDOW_FIELDS)
  texts     6 B  CompactTextInfo   r, g, b, size, align, filter_count
  filters  12 B  CompactFilterInfo f32 strength, u8 blur, s8 dx, s8 dy, u8 knockout, u8 r, g, b, pad
  strings   1 B  NUL-separated names referenced by byte offset
Window type (low byte of u16[9]): 0 Window, 1 Button, 2 Textfield (consumes the next text record,
which consumes filter_count filter records). Full name = parent full name + "." + own name.
Scale-9 insets: left = byte 19, top = byte 20, right = u16[11], bottom = u16[12].
"""
import json, struct, sys

TYPES = {0: "window", 1: "button", 2: "textfield"}


def parse(buf):
    p = 0

    def section(size):
        nonlocal p
        n, = struct.unpack_from("<I", buf, p); p += 4
        data = buf[p:p + n * size]; p += n * size
        return n, data

    nw, wdata = section(26)
    nt, tdata = section(6)
    nf, fdata = section(12)
    ns, sdata = section(1)
    if p != len(buf):
        raise ValueError(f"{len(buf) - p} trailing bytes")

    def s(off):
        return sdata[off:sdata.index(b"\0", off)].decode("latin1")

    texts = [dict(zip(("r", "g", "b", "size", "align", "filters"), tdata[i * 6:i * 6 + 6])) for i in range(nt)]
    filters = []
    for i in range(nf):
        strength, blur, dx, dy, knock, r, g, b = struct.unpack_from("<fBbbBBBB", fdata, i * 12)
        filters.append(dict(strength=strength, blur=blur, dx=dx, dy=dy, knockout=bool(knock), color=(r, g, b)))

    windows, ti, fi = [], 0, 0
    for i in range(nw):
        u = struct.unpack_from("<13H", wdata, i * 26)
        raw = wdata[i * 26:i * 26 + 26]
        w = dict(index=i, name=s(u[0]), symbol=s(u[1]), parent=u[2], image=u[3] - 1 if u[3] else None,
                 x=struct.unpack("<h", raw[8:10])[0], y=struct.unpack("<h", raw[10:12])[0],
                 w=struct.unpack("<h", raw[12:14])[0], h=struct.unpack("<h", raw[14:16])[0],
                 u8=u[8], type=TYPES.get(raw[18], raw[18]),
                 scale9=(raw[19], raw[20], u[11], u[12]) if (raw[19] or raw[20] or u[11] or u[12]) else None)
        if raw[18] == 2:
            t = dict(texts[ti]); ti += 1
            t["filters"] = filters[fi:fi + t["filters"]]; fi += len(t["filters"])
            w["text"] = t
        windows.append(w)
    for w in windows:  # full dotted names (root keeps its own name)
        w["full"] = w["name"] if w["index"] == 0 or w["parent"] == 0 and w["index"] == 0 else (
            (windows[w["parent"]]["full"] + "." if w["parent"] else "") + w["name"])
    return dict(windows=windows, texts_used=ti, texts=nt, filters_used=fi, filters=nf)


def check(d):
    """Internal consistency: parents precede children, all texts/filters consumed exactly once."""
    errs = []
    for w in d["windows"]:
        if w["index"] and w["parent"] >= w["index"]:
            errs.append(f"window {w['index']} parent {w['parent']} not before it")
        if w["type"] not in TYPES.values():
            errs.append(f"window {w['index']} unknown type {w['type']}")
    if d["texts_used"] != d["texts"]:
        errs.append(f"texts used {d['texts_used']} of {d['texts']}")
    if d["filters_used"] != d["filters"]:
        errs.append(f"filters used {d['filters_used']} of {d['filters']}")
    return errs


if __name__ == "__main__":
    import glob, os
    if len(sys.argv) == 2 and sys.argv[1].endswith(".xmlb"):
        print(json.dumps(parse(open(sys.argv[1], "rb").read()), indent=1))
        sys.exit()
    files = sorted(f for a in sys.argv[1:] for f in glob.glob(os.path.join(a, "*.xmlb")))
    bad = 0
    for f in files:
        try:
            errs = check(parse(open(f, "rb").read()))
        except Exception as e:
            errs = [repr(e)]
        if errs:
            bad += 1
            print(os.path.basename(f), errs[:3])
    print(f"{len(files) - bad}/{len(files)} consistent")
