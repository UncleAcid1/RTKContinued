#!/usr/bin/env python3
"""Extract Rule the Kingdom resource containers.

Supported sources (all share the same index-entry layout):
  android  <dir>   assets/data/ with res_desc.bin + <id>.jet chunks
  win8     <dir>   Assets/ with res_desc.bin + res_data.bin
  kbf      <file>  expansion pack (*.kbf)

Index entry: u32 name_len (incl. NUL), name, then 8 x u32 LE:
  hash, width, height, is_data, bpp, offset, size, chunk
  - offset is global across the whole data set
  - chunk is the .jet file id on Android (unused elsewhere)
Gzipped payloads (1f 8b) are decompressed unless --raw.
"""
import argparse, gzip, os, struct, sys
from collections import defaultdict

KBF_MAGIC = 0x75B4A351
FIELDS = ("hash", "width", "height", "is_data", "bpp", "offset", "size", "chunk")


def parse_index(buf, start=0, end=None):
    end = len(buf) if end is None else end
    p, entries = start, []
    while p < end:
        n, = struct.unpack_from("<I", buf, p); p += 4
        name = buf[p:p + n].rstrip(b"\0").decode("latin1"); p += n
        e = dict(zip(FIELDS, struct.unpack_from("<8I", buf, p))); p += 32
        e["name"] = name
        entries.append(e)
    if p != end:
        raise ValueError(f"index ended at {p}, expected {end}")
    return entries


def iter_android(path):
    entries = parse_index(open(os.path.join(path, "res_desc.bin"), "rb").read())
    by_chunk = defaultdict(list)
    for e in entries:
        by_chunk[e["chunk"]].append(e)
    for chunk, es in by_chunk.items():
        blob = open(os.path.join(path, f"{chunk}.jet"), "rb").read()
        base = min(e["offset"] for e in es)
        for e in es:
            o = e["offset"] - base
            yield e, blob[o:o + e["size"]]


def iter_win8(path):
    entries = parse_index(open(os.path.join(path, "res_desc.bin"), "rb").read())
    data = open(os.path.join(path, "res_data.bin"), "rb").read()
    for e in entries:
        yield e, data[e["offset"]:e["offset"] + e["size"]]


def iter_kbf(path):
    buf = open(path, "rb").read()
    magic, index_size = struct.unpack_from("<II", buf, 0)
    if magic != KBF_MAGIC:
        raise ValueError(f"{path}: bad magic {magic:#x}")
    base = 20 + index_size
    for e in parse_index(buf, 20, base):
        yield e, buf[base + e["offset"]:base + e["offset"] + e["size"]]


def out_name(name):
    return name[len("../resource/"):] if name.startswith("../resource/") else name


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("kind", choices=("android", "win8", "kbf"))
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--raw", action="store_true", help="do not gunzip payloads")
    ap.add_argument("--list", action="store_true", help="only print the index as TSV")
    a = ap.parse_args()

    it = {"android": iter_android, "win8": iter_win8, "kbf": iter_kbf}[a.kind](a.src)
    count = gz = 0
    if a.list:
        print("\t".join(("name",) + FIELDS))
    for e, blob in it:
        if len(blob) != e["size"]:
            sys.exit(f"truncated payload: {e['name']}")
        count += 1
        if a.list:
            print("\t".join([e["name"]] + [str(e[f]) for f in FIELDS]))
            continue
        if not a.raw and blob[:2] == b"\x1f\x8b":
            blob = gzip.decompress(blob); gz += 1
        dst = os.path.join(a.dst, out_name(e["name"]))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with open(dst, "wb") as f:
            f.write(blob)
    print(f"{count} entries ({gz} gunzipped)", file=sys.stderr)


if __name__ == "__main__":
    main()
