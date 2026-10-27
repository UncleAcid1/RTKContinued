#!/usr/bin/env python3
"""Makes gamedata/win8_supplement.kbf: the files that only the Windows 8 build has (218 entries:
shop, chest, set, PvP and profession art the 5.11 APK lacks), packed as a .kbf the game registers
after the two update packs. It replaces the old fallback on the whole Windows 8 Appx
(FileManager::RegisterWin8Fallback added only names nothing else had, so the result is the same).

Each entry keeps its original index fields (hash, size, flags, ...) and its payload bytes as stored
(gzipped where the Appx had them gzipped); only the offset is new.

Usage: tools/make_supplement_pack.py [BACKUP_ROOT]   (default: .., the folder with "Android files",
       "files" and "_extract")
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rtk_extract as rx  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "gamedata", "win8_supplement.kbf")


def kbf_names(path):
    buf = open(path, "rb").read()
    n = struct.unpack_from("<I", buf, 4)[0]
    return {e["name"].lower() for e in rx.parse_index(buf, 20, 20 + n)}


def main():
    backup = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "..")
    apk = os.path.join(backup, "Android files", "rule-the-kingdom-5-11-multi-android", "assets", "data")
    have = {e["name"].lower() for e in rx.parse_index(open(os.path.join(apk, "res_desc.bin"), "rb").read())}
    have |= kbf_names(os.path.join(backup, "files", "main.27.kbf"))
    have |= kbf_names(os.path.join(backup, "files", "patch.33.kbf"))

    index, data = bytearray(), bytearray()
    count = 0
    for e, payload in rx.iter_win8(os.path.join(backup, "_extract", "win5", "Assets")):
        if e["name"].lower() in have:
            continue
        have.add(e["name"].lower())
        name = e["name"].encode("latin1") + b"\0"
        index += struct.pack("<I", len(name)) + name
        index += struct.pack("<8I", e["hash"], e["width"], e["height"], e["is_data"], e["bpp"], len(data),
                             len(payload), 0)
        data += payload
        count += 1

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(struct.pack("<5I", rx.KBF_MAGIC, len(index), 0, len(data), 0))
        f.write(index)
        f.write(data)
    print(f"wrote {OUT}: {count} files, {os.path.getsize(OUT)} bytes")


if __name__ == "__main__":
    main()
