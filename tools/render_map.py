#!/usr/bin/env python3
"""Sprite-accurate static render of a Rule the Kingdom map.

Every formula below is transcribed from libkingdom.so 5.11 (addresses in comments).
What is NOT rendered: characters/units, animations (frame 0 only), fog, HUD,
particles, and the terrain colour-mask shader (its mask texture is not yet identified).
Per-player randomness (Map::Patch::AddRandomDecors, dark-grass patches) uses
GameState::playerSeed, which differs per player; pass --seed to choose one.
"""
import argparse, os, re, sys
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import rtk_map

HERE = os.path.dirname(os.path.abspath(__file__))
ASSETS = os.path.join(HERE, "../out/assets/android511")
RES = os.path.join(ASSETS, "res_files/1Original")


# ---------------------------------------------------------------- bionic lrand48
class Rand48:
    def __init__(self):
        self.x = 0x1234ABCD330E

    def srand48(self, seed):
        self.x = ((seed & 0xFFFFFFFF) << 16) | 0x330E

    def lrand48(self):
        self.x = (0x5DEECE66D * self.x + 0xB) & ((1 << 48) - 1)
        return self.x >> 17


# ---------------------------------------------------------------- resources
class Resources:
    """Resources::GetImage / GetDecoratedImageName / CreateTexture (_.png alpha)."""
    def __init__(self, root):
        self.root, self.cache = root, {}
        # AllAnimsFrames.xml: onefile animations live in A3MergedAnims as "<name>_anim" (GetImage "%s_anim")
        text = open(os.path.join(RES, "AllAnimsFrames.xml"), encoding="utf-8", errors="replace").read()
        self.anims = {m.group(1).lower(): int(m.group(2))
                      for m in re.finditer(r"<anim name='([^']+)' frames='(\d+)' onefile='true'", text)}

    def _anim_frame0(self, name, frames):  # Render::Sprite::SetFrame 0x1fe940, frame 0
        path = self._decorated("A3MergedAnims", name + "_anim")
        if not path:
            return None
        sheet = self._load(path)
        w, h = sheet.size
        if "tree5_stump" in name or "fir5_stump" in name:  # GetImage forces 1 frame
            frames = 1
        if h > 2048 and frames >= 2:  # two columns, ceil(frames/2) per column
            per_col = (frames + 1) >> 1
            return sheet.crop((0, 0, w // 2, h // per_col))
        return sheet.crop((0, 0, w, h // frames))

    def _decorated(self, pack, name):  # 0x203c64
        for fmt in ("{p}/2Optimized/{n}.png", "{p}/2Optimized/{n}.jpg", "{p}/1Original/{n}.png", "{p}/1Original/{n}.jpg"):
            path = os.path.join(self.root, fmt.format(p=pack, n=name))
            if os.path.exists(path):
                return path
        path = os.path.join(self.root, name)
        return path if os.path.exists(path) else None

    def _load(self, path):  # CreateTexture: replace extension with "_.png" for alpha
        img = Image.open(path).convert("RGB")
        alpha = os.path.splitext(path)[0] + "_.png"
        if os.path.exists(alpha):
            a = Image.open(alpha).convert("L")
            if a.size == img.size:
                img.putalpha(a)
                return img
        return Image.open(path).convert("RGBA")

    def image(self, name):  # 0x204304: A2Static, A3MergedAnims, then <name>_00 (anim frame 0)
        if name in self.cache:
            return self.cache[name]
        path = self._decorated("A2Static", name) or self._decorated("A3MergedAnims", name)
        if path:
            img = self._load(path)
        elif name.lower() in self.anims:
            img = self._anim_frame0(name, self.anims[name.lower()])
        else:
            path = self._decorated("A3MergedAnims", name + "_00")
            img = self._load(path) if path else None
        self.cache[name] = img
        return img

    def decoration(self, img):  # 0x204b24
        for fmt in ("images/Decor/%s", "images/Buildings/%s", "images/%s", "%s"):
            im = self.image(fmt % img)
            if im is not None:
                return im
        return None


def load_defs(fname, tag):
    text = open(os.path.join(RES, fname), encoding="utf-8", errors="replace").read()
    out = {}
    for m in re.finditer(rf"<{tag}\b([^>]*?)/?>(.*?)(?=<{tag}\b|</\w+Options|</\w+>\s*$|\Z)", text, re.S):
        a = dict(re.findall(r"(\w+)=['\"]([^'\"]*)['\"]", m.group(1)))
        if "id" in a:
            a["_floors"] = [dict(re.findall(r"(\w+)=['\"]([^'\"]*)['\"]", f)) for f in re.findall(r"<floor\b([^>]*)>", m.group(2))]
            out[int(a["id"])] = a
    return out


def ival(d, k, default=0):
    try:
        return int(d.get(k, default) or default)
    except ValueError:
        return default


def tile_world(x, y):  # Map::TileCoordinatesToWorld 0x1b632c
    return x * 84 + (42 if y & 1 else 0), y * 21


def sprite_z(a, b):  # Map::GetSpriteZ 0x1b6814 (c%5 term omitted: register not set by callers)
    v = (a - b * 0.25) / 15.0 / 1000.0
    return 0.5 - v if v <= 0.2 else 0.3


# ---------------------------------------------------------------- map object model
class Scene:
    def __init__(self):
        self.layers = {}  # layer -> list of (z, seq, img, x, y, mirror)
        self.seq = 0

    def add(self, layer, img, x, y, z=0.0, mirror=False):
        if img is None:
            return
        self.seq += 1
        self.layers.setdefault(layer, []).append((z, self.seq, img, x, y, mirror))


def footprint(bx, by, w, h):  # Map::Decor::UpdateMapLink 0x1346b8 tile walk
    x, y = bx, by
    for i in range(w // 2):
        if ((by + i) & 1) == 0:
            x -= 1
    y += w // 2
    tiles = []
    for _ in range(h):
        cx, cy = x, y
        for _ in range(w):
            tiles.append((cx, cy))
            odd = cy & 1
            cy -= 1
            if odd:
                cx += 1
        if (y & 1) == 0:
            x -= 1
        y -= 1
    return tiles


def render(map_path, seed, out_full, out_crop, crop_area, scale_full):
    m = rtk_map.decode_map(open(map_path, "rb").read())
    hdr = m["header"]
    gw, gh, map_id = hdr["grid_w"], hdr["grid_h"], hdr["map_id"]
    R = Resources(ASSETS)
    decors = load_defs("decors.xml", "Decor")
    buildings = load_defs("buildings.xml", "Building")
    areas = {int(a["id"]): a for a in [dict(re.findall(r"(\w+)=\"([^\"]*)\"", s))
             for s in re.findall(r"<ar\b([^>]*)>", open(os.path.join(RES, "dynamic_config.xml"), encoding="utf-8").read())]}
    scene, rnd = Scene(), Rand48()
    occupied = set()

    # --- ground: Background::Init 0x10c308 loads images/grass_01.png (tileset 0); CreateLand 0x10b9ec tiles it
    #     (wrapping, anchored at (-20*texW, -2*texH), i.e. aligned to world origin), layer 0.
    ground = Image.open(os.path.join(ASSETS, "images/grass_01.png")).convert("RGBA")
    #     Horizon (non-mission maps, 0x10bfa0): images/Tileset/summer/top repeated 6x, centred on the map,
    #     bottom edge at y = -40.5, layer 0 after the ground.
    top = R.image("images/Tileset/summer/top")
    scene.add(0, Image.new("RGBA", (1, 1)), 0, 0)  # placeholder keeps layer 0 present
    if top is not None:
        strip = Image.new("RGBA", (top.size[0] * 6, top.size[1]))
        for i in range(6):
            strip.paste(top, (i * top.size[0], 0))
        scene.add(0, strip, int(gw * 42 - 3 * top.size[0]), -40)

    # --- patches (areas from dynamic_config <ar>), ownership flags from the map file
    patches = []
    for p in m["patches"]:
        a = areas[p["area_id"]]
        patches.append(dict(p, px=int(a["x"]), py=int(a["y"]), pw=int(a["w"]), ph=int(a["h"]), buy=a.get("a") == "1"))

    # --- map decors: LoadDecors + Decor::UpdateImage 0x134c14 (depthStyle == 0, verified never written)
    def place_decor(d, layer_override=None):
        dd = decors.get(d["decor_id"])
        if dd is None:
            return
        img = R.decoration(dd.get("img", ""))
        w, h, ox, oy = ival(dd, "lockzoneX"), ival(dd, "lockzoneY"), ival(dd, "x"), ival(dd, "y")
        for t in footprint(d["x"], d["y"], w, h):
            occupied.add(t)
        if img is None:
            return
        wx, wy = tile_world(d["x"], d["y"])
        sw = img.size[0]
        if not d.get("flag_1c"):
            X = wx + 42 - 42 * int(w / 2) - ox
            Y = wy + 21 * int(h / 2) - oy
        else:  # mirrored branch 0x134ecc
            dd_ = (w - h) if ((w > h and w % 2 == 0) or (w < h and w % 2 == 1)) else 0
            dd_ = 2 if dd_ == 3 else dd_
            X = wx + 42 - 42 * int(w / 2) + ox + 42 * dd_
            Y = wy + 21 * int(h / 2) - oy - 21 * dd_
        X -= int(sw * 0.5)
        layer = {-1: 11, 0: 8, 1: 3, 2: 3, 3: 2}.get(ival(dd, "layer"), 8) if layer_override is None else layer_override
        scene.add(layer, img, X, Y, sprite_z(wy, sw * 0.5), mirror=bool(d.get("flag_1c")))

    for p in m["patches"]:
        for d in p["decors"]:
            place_decor(d)

    # --- map buildings: Building::FindBaseCoordinates 0x11cbb8 + CreateBuildingPartSprite 0x11ef2c
    for p in m["patches"]:
        for b in p["buildings"]:
            bd = buildings.get(b["building_id"] if b["building_id"] != 100 else 99)
            if bd is None:
                continue  # undefined ids are skipped by the game too
            bzx, bzy = ival(bd, "buildZoneX"), ival(bd, "buildZoneY")
            for t in footprint(b["x"], b["y"], bzx, bzy):
                occupied.add(t)
            wx, wy = tile_world(b["x"], b["y"])
            baseX = wx + 42 - 42 * (bzx >> 1) - ival(bd, "offsetX")
            baseY = wy + 21 * (bzy >> 1) - ival(bd, "offsetY")
            # floor stages (LoadBuildingList 0x122ae4): trees/149/1000 special, else order index
            floors, counter = [], 0
            for f in bd["_floors"]:
                if bd["id"] in ("17", "149", "1000"):
                    if "stump" in f.get("part_img", ""):
                        stg = 6; counter = 0
                    elif "cut" in f.get("part_img", ""):
                        stg = 5
                    else:
                        stg = counter; counter += 1
                else:
                    stg = None
                floors.append((stg, f))
            level = b["u50"]
            chosen = []
            while level >= 0 and not chosen:  # FilterBuildingParts, falling back to lower levels
                idx = 0
                for stg, f in floors:
                    if stg is None:
                        if idx == level:
                            chosen.append(f)
                        idx += 1
                    elif stg == level:
                        chosen.append(f)
                level -= 1
            minx = maxx = baseX; maxbot = baseY
            for i, f in enumerate(chosen):
                img = R.image(f["part_img"])
                if img is None:
                    continue
                X = baseX - img.size[0] * 0.5 + ival(f, "off_x")
                Y = baseY + ival(f, "off_y")
                scene.add(8, img, int(X), Y, sprite_z(baseY + 0.1 * i, 0))
                minx, maxx, maxbot = min(minx, X), max(maxx, X + img.size[0]), max(maxbot, Y)
            if ival(bd, "building_class") == 4:  # ring 0x12a050
                ring = R.image("images/Rings/under_rings_tree_rock")
                dx, dy = {17: (-1, -12), 20: (-12, -25)}.get(int(bd["id"]), (0, 0))
                X = (minx + maxx) * 0.5 - ring.size[0] * 0.5 + dx
                Y = maxbot + ring.size[1] * 0.5 + dy
                scene.add(7, ring, int(X), int(Y))

    # --- random decors: Map::Patch::AddRandomDecors 0x1e3a9c (map 0, tileset 0 only)
    if map_id == 0 and hdr["tileset"] == 0:
        ids = [0x12, 0x16, 0x20, 0x22, 0x23, 0x24, 0x25, 0x27, 0x28, 0x2a, 0x2e, 0x2f, 0x30, 0x31, 0x32]
        for p in patches:
            rnd.srand48(seed)
            for _ in range((p["pw"] * p["ph"]) // 35):
                x = p["px"] + rnd.lrand48() % (p["pw"] - 1)
                y = p["py"] + rnd.lrand48() % (p["ph"] - 1)
                if (x, y) in occupied:
                    continue
                did = ids[rnd.lrand48() % len(ids)]
                place_decor(dict(x=x, y=y, decor_id=did, flag_1c=False))

    # --- area borders: Map::UpdateAreaBorders 0x1bcda8
    for p in patches:  # e9 recomputed: owned, or adjacent to an owned patch
        p["adj"] = p["flag_e8"] or any(
            q is not p and q["flag_e8"] and (
                (p["px"] in (q["px"] + q["pw"],) or q["px"] == p["px"] + p["pw"]) and p["py"] == q["py"]
                or p["py"] == q["py"] + q["ph"] and p["px"] == q["px"]) for q in patches)
    sign = R.image("images/Buildings/land_expand")
    for p in patches:  # for-sale signs, layer 8
        if not p["flag_e8"] and p["px"] + p["pw"] <= gw and p["adj"] and p["buy"]:
            X, Y = (p["px"] + p["pw"] // 2) * 84, (p["py"] + p["ph"] // 2) * 21
            scene.add(8, sign, int(X), int(Y), sprite_z(Y, sign.size[0] * 0.5))
    rnd.srand48(seed)  # dark grass patches, layer 1
    grass = [R.image(f"images/Tileset/summer/grass_dark_big_00{i}") for i in range(1, 7)]
    for _ in range((gw * gh) // 60):
        r1, r2 = rnd.lrand48(), rnd.lrand48()
        y = r2 % (gh - 6) + 3
        g = grass[rnd.lrand48() % 6]
        if g is None:
            continue
        x = r1 % (gw - 3)
        scene.add(1, g, x * 84 + (42 if y & 1 else 0) + 42, y * 21)
    flag = Image.open(os.path.join(ASSETS, "images/land_expand_flag.png")).convert("RGBA")
    for p in patches:  # flag posts, layer 3
        if p["flag_e8"] or not p["adj"]:
            continue
        for y in range(p["py"], p["py"] + p["ph"] + 1):
            for x in range(p["px"], p["px"] + p["pw"] + 1):
                left, right = x == p["px"], x == p["px"] + p["pw"]
                if (left and y % 2 == 0) or (right and y % 2 == 0) or (y == p["py"] and y != 0) or y == p["py"] + p["ph"]:
                    X = x * 84 + (42 if y & 1 else 0) + 42 - int(flag.size[0] * 0.5)
                    scene.add(3, flag, X, y * 21 - 21)

    # --- compose: layers ascending; layer 8 sorted by SpriteSortZ (higher z first)
    W, H = gw * 84 + 84, gh * 21 + 42
    pad = 200
    canvas = Image.new("RGBA", (W + 2 * pad, H + 2 * pad), (0, 0, 0, 255))
    tw, th = ground.size
    for ty in range(-(pad // th) - 1, (H + 2 * pad) // th + 2):
        for tx in range(-(pad // tw) - 1, (W + 2 * pad) // tw + 2):
            canvas.paste(ground, (tx * tw + pad % tw, ty * th + pad % th))
    for layer in sorted(scene.layers):
        items = scene.layers[layer]
        if layer == 8:
            items = sorted(items, key=lambda t: (-t[0], t[1]))
        for z, _, img, X, Y, mirror in items:
            im = img.transpose(Image.FLIP_LEFT_RIGHT) if mirror else img
            canvas.alpha_composite(im, (int(X) + pad, int(Y) - im.size[1] + pad))  # (X,Y) = bottom-left

    canvas = canvas.convert("RGB")
    full = canvas.resize((int(canvas.size[0] * scale_full), int(canvas.size[1] * scale_full)), Image.LANCZOS)
    full.save(out_full)
    if crop_area:
        a = areas[crop_area]
        x0, y0 = int(a["x"]) * 84 + pad - 300, int(a["y"]) * 21 + pad - 150
        x1, y1 = (int(a["x"]) + int(a["w"])) * 84 + pad + 300, (int(a["y"]) + int(a["h"])) * 21 + pad + 250
        canvas.crop((x0, y0, x1, y1)).save(out_crop)
    print("layers:", {k: len(v) for k, v in sorted(scene.layers.items())})


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("map")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default="map_full.png")
    ap.add_argument("--crop-out", default="map_crop.png")
    ap.add_argument("--crop-area", type=int, default=1)
    ap.add_argument("--scale", type=float, default=0.25)
    a = ap.parse_args()
    render(a.map, a.seed, a.out, a.crop_out, a.crop_area, a.scale)
