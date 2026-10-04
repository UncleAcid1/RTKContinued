#!/usr/bin/env python3
"""Rule the Kingdom map / save chunk format.

Translated from libkingdom.so 5.11:
  SaveManager::ConvertOldFormatMap   (old map layout -> chunks)
  SaveManager::SaveBlock::ParseDataIntoChunks (chunk stream)

Chunk stream: repeated [u32 type BE][u32 size BE][payload], terminated by type 0x13.
Old-format maps start with u32 1, u32 map_id; ConvertOldFormatMap re-chunks them
(the 8-byte header is part of the first 13-byte header chunk).
"""
import struct, sys

END = 0x13


class Reader:
    def __init__(self, buf):
        self.buf, self.p = buf, 0

    def bytes(self, n):
        if n < 0 or self.p + n > len(self.buf):
            raise ValueError(f"read {n} at {self.p} past end {len(self.buf)}")
        b = self.buf[self.p:self.p + n]; self.p += n
        return b

    def u8(self):
        return self.bytes(1)[0]

    def s16(self):
        return struct.unpack(">h", self.bytes(2))[0]

    def u32(self):
        return struct.unpack(">I", self.bytes(4))[0]


def convert_old_format(buf):
    """Exact port of SaveManager::ConvertOldFormatMap. Returns [(type, payload)]."""
    r, out = Reader(buf), []

    def chunk(t, *parts):
        out.append((t, b"".join(parts)))

    def s16_and_blob():
        n = r.s16()
        return struct.pack(">h", n) + r.bytes(n)

    hdr = r.bytes(0xd); n_patches = r.u8()
    chunk(0xe, hdr, bytes([n_patches]))
    for _ in range(n_patches):
        chunk(4, r.bytes(3))
        n = r.s16(); chunk(5, struct.pack(">h", n))
        for _ in range(n):
            chunk(0xb, r.bytes(0x14), s16_and_blob(), s16_and_blob())
        n = r.s16(); chunk(6, struct.pack(">h", n))
        for _ in range(n):
            a = r.bytes(0x2e); k = r.u8()
            chunk(0xc, a, bytes([k]), r.bytes(k * 2), r.bytes(0xe),
                  s16_and_blob(), s16_and_blob(), r.bytes(4))
        chunk(7, s16_and_blob())
    n = r.s16(); chunk(2, struct.pack(">h", n))
    for _ in range(n):
        chunk(9, r.bytes(0xf), s16_and_blob())
    n = r.s16(); chunk(3, struct.pack(">h", n))
    for _ in range(n):
        chunk(0xa, r.bytes(0xb))
    n = r.s16(); chunk(0x17, struct.pack(">h", n), r.bytes(n * 3))
    chunk(END, struct.pack(">I", 0))
    return out, r.p


def parse_chunks(buf):
    """Exact port of ParseDataIntoChunks (new format). Returns ([(type, payload)], bytes_used)."""
    r, out = Reader(buf), []
    while True:
        t, n = r.u32(), r.u32()
        out.append((t, r.bytes(n)))
        if t == END:
            return out, r.p


def load(buf, map_id=None):
    """Mirror the loader: old format if header is (1, map_id)."""
    a, b = struct.unpack_from(">II", buf, 0)
    if a == 1 and (map_id is None or b == map_id):
        return ("old",) + convert_old_format(buf)
    return ("new",) + parse_chunks(buf)


# ---- Semantic decoding (field order from Map::LoadPlayer / LoadDecors / LoadBuidings /
# ---- LoadSpawnPoints / LoadPortals / Patch::Load). Unknown fields keep their offset name.

def _str(r):
    return r.bytes(r.s16()).decode("latin1")


def decode_header(p):  # chunk 0xe, LoadPlayer
    r = Reader(p)
    return dict(version=r.u32(), map_id=r.u32(), grid_w=r.u8(), grid_h=r.u8(),
                pos_x=struct.unpack("b", r.bytes(1))[0], pos_y=struct.unpack("b", r.bytes(1))[0],
                tileset=r.u8(), patch_count=r.u8())


def decode_patch(p):  # chunk 4
    return dict(area_id=p[0], flag_e8=bool(p[1]), flag_e9=bool(p[2]))


def decode_decor(p):  # chunk 0xb, LoadDecors
    r = Reader(p)
    d = dict(x=r.u8(), y=r.u8(), decor_id=r.u32())
    f = r.u8()
    d.update(flag_1c=bool(f & 1), flag_44=bool(f & 2), u48=r.u32(), u4c=r.u8(),
             u50=r.u32(), u54=r.u32(), resources=_str(r), meta=_str(r))
    assert r.p == len(p)
    return d


def decode_building(p):  # chunk 0xc, LoadBuidings
    r = Reader(p)
    d = dict(x=r.u8(), y=r.u8(), building_id=r.u32(), u40=r.u32(), u48=r.u32(), u54=r.u32())
    six = r.bytes(6)
    d.update(flag_1c=bool(six[0]), u50=six[2], skip6=six.hex(), skip_a=r.u32(), skip_b=r.u32())
    d.update(flag_e4=bool(r.u8()), ue8=r.u8(), uf8=r.u32(), ufc=r.u32(), u100=r.u32())
    d["shorts"] = [r.s16() for _ in range(r.u8())]
    d.update(u104=r.u8(), u108=r.u32(), u10c=r.u8(), u110=r.u32(), u114=r.u32(),
             str118=_str(r), meta=_str(r), u58=r.u32())
    assert r.p == len(p)
    return d


def decode_spawn(p):  # chunk 9, LoadSpawnPoints
    r = Reader(p)
    d = dict(id=r.u32(), s1=r.s16())
    b = r.bytes(9)
    d.update(b2=b[0], b3=b[1], flag4=bool(b[2]), b5=b[3], b6=b[4], b7=b[5], flag8=bool(b[6]),
             b9=b[7], b10=b[8], meta=_str(r))
    assert r.p == len(p)
    return d


def decode_portal(p):  # chunk 0xa, LoadPortals
    r = Reader(p)
    d = dict(s0=r.s16(), b1=r.u8(), s2=r.s16(), s3=r.s16(), b=list(r.bytes(4)))
    assert r.p == len(p)
    return d


def decode_mask(p):  # chunk 7, Patch::Load: '0'/'1' per tile, x-major
    r = Reader(p)
    return r.bytes(r.s16()).decode("latin1")


def decode_fog(p):  # chunk 0x17, LoadPlayer -> AddFog(x, y, type)
    n = struct.unpack(">H", p[:2])[0]
    return [tuple(p[2 + 3 * i:5 + 3 * i]) for i in range(n)]


def decode_map(buf):
    """Decode a map into a dict. Only the chunk layout produced by maps is handled."""
    kind, chunks, used = load(buf)
    if used != len(buf):
        raise ValueError(f"{len(buf) - used} trailing bytes")
    m = dict(format=kind, patches=[], spawns=[], portals=[], fog=[])
    cur = None
    for t, p in chunks:
        if t == 0xe:
            m["header"] = decode_header(p)
        elif t == 4:
            cur = dict(decode_patch(p), decors=[], buildings=[], mask="")
            m["patches"].append(cur)
        elif t == 0xb:
            cur["decors"].append(decode_decor(p))
        elif t == 0xc:
            cur["buildings"].append(decode_building(p))
        elif t == 7:
            cur["mask"] = decode_mask(p)
        elif t == 9:
            m["spawns"].append(decode_spawn(p))
        elif t == 0xa:
            m["portals"].append(decode_portal(p))
        elif t == 0x17:
            m["fog"] = decode_fog(p)
    return m


if __name__ == "__main__":
    import collections, os
    for path in sys.argv[1:]:
        files = [os.path.join(path, f) for f in sorted(os.listdir(path))] if os.path.isdir(path) else [path]
        ok = 0
        for f in files:
            buf = open(f, "rb").read()
            try:
                kind, chunks, used = load(buf)
                types = collections.Counter(t for t, _ in chunks)
                status = "OK" if used == len(buf) else f"LEFTOVER {len(buf) - used}"
                ok += used == len(buf)
                print(f"{os.path.basename(f):16} {kind} {len(buf):6}B {status:14} chunks={dict(sorted(types.items()))}")
            except Exception as e:
                print(f"{os.path.basename(f):16} FAIL {e}")
        print(f"{ok}/{len(files)} consumed exactly")
