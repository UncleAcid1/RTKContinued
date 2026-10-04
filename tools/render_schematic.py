#!/usr/bin/env python3
"""Schematic map preview: exact tile positions, no sprites.

Uses Map::TileCoordinatesToWorld (staggered iso: x*84 + (y odd)*42, y*21).
Decor colour is chosen from its decors.xml name/img; this is a visual aid only.
"""
import argparse, re, sys, os
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(__file__))
import rtk_map

TW, TH = 84, 42  # tile diamond size (influence_border_.png is 84x42)


def tile_to_world(x, y):
    return x * TW + (TW // 2 if y & 1 else 0), y * (TH // 2)


def load_defs(xml_path, tag):
    defs = {}
    for m in re.finditer(rf"<{tag}\b([^>]*)>", open(xml_path, encoding="utf-8", errors="replace").read()):
        attrs = dict(re.findall(r"(\w+)=['\"]([^'\"]*)['\"]", m.group(1)))
        if "id" in attrs:
            defs[int(attrs["id"])] = attrs
    return defs


def colour(d):
    s = (d.get("name", "") + " " + d.get("img", "")).lower()
    if d.get("isroad") == "1" or "road" in s or "footpath" in s:
        return (200, 180, 130)
    for keys, c in ((("pine", "fir", "spruce"), (40, 110, 40)), (("tree", "oak", "bush"), (110, 170, 50)),
                    (("rock", "stone", "cliff", "boulder"), (150, 150, 150)), (("water", "lake", "river"), (60, 120, 200)),
                    (("fence", "wall", "barr"), (140, 90, 50)), (("flower", "grass", "plane"), (170, 200, 90))):
        if any(k in s for k in keys):
            return c
    return (230, 120, 200)


def diamond(cx, cy, w=TW, h=TH):
    return [(cx, cy - h // 2), (cx + w // 2, cy), (cx, cy + h // 2), (cx - w // 2, cy)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("map"); ap.add_argument("out")
    ap.add_argument("--res", default=os.path.join(os.path.dirname(__file__), "../out/assets/android511/res_files/1Original"))
    ap.add_argument("--scale", type=float, default=0.25)
    a = ap.parse_args()

    m = rtk_map.decode_map(open(a.map, "rb").read())
    h = m["header"]
    decors = load_defs(os.path.join(a.res, "decors.xml"), "Decor")
    W, H = h["grid_w"] * TW + TW, h["grid_h"] * TH // 2 + TH
    img = Image.new("RGB", (W, H), (60, 75, 40))
    dr = ImageDraw.Draw(img)
    ox, oy = TW // 2, TH // 2

    for x in range(h["grid_w"]):  # base grid, blocked tiles darker
        for y in range(h["grid_h"]):
            blocked = any(p["mask"] and p["mask"][x * h["grid_h"] + y] == "1" for p in m["patches"])
            wx, wy = tile_to_world(x, y)
            dr.polygon(diamond(wx + ox, wy + oy), fill=(45, 50, 35) if blocked else (95, 125, 60), outline=(80, 105, 50))

    for p in m["patches"]:
        for d in sorted(p["decors"], key=lambda d: d["y"]):
            wx, wy = tile_to_world(d["x"], d["y"])
            dd = decors.get(d["decor_id"], {})
            dr.polygon(diamond(wx + ox, wy + oy, TW - 16, TH - 8), fill=colour(dd))
        for b in p["buildings"]:
            wx, wy = tile_to_world(b["x"], b["y"])
            dr.polygon(diamond(wx + ox, wy + oy, TW * 3, TH * 3), outline=(255, 40, 40), width=6)
    for s in m["spawns"]:
        pass  # spawn coordinates not yet identified among the spawn fields
    out = img.resize((int(W * a.scale), int(H * a.scale)), Image.LANCZOS)
    out.save(a.out)
    print(f"map {h['map_id']} {h['grid_w']}x{h['grid_h']} -> {a.out} {out.size}")


if __name__ == "__main__":
    main()
