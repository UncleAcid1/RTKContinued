#!/usr/bin/env python3
"""Draws the Mac disk image's window background (packaging/macos/dmg/background.png and
background@2x.png) from the original game's art: the world map (images/WorldMap/world_map_bg.jpg),
the game's large logo (kingdom_ui/2Optimized/..._logo_large_eng.jpg + its alpha mask), a band of mist, and a gold arrow from the game to Applications, with
the game's own font (fonts/ARICYRB.TTF).

The icon positions must match tools/make_dmg.sh (APP_X, APPS_X, ICON_Y). Finder writes the icons'
names in black (light mode) or white (dark mode), so they sit on a mid-toned band of mist where
either reads.

Needs the extracted 5.11 assets in out/assets/android511 (README "Regenerate") and Pillow
(pip install pillow).

Usage: tools/make_dmg_background.py
"""
import os
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
A = os.path.join(ROOT, "out", "assets", "android511") + "/"
LOGO = A + "kingdom_ui/2Optimized/Window_chapters___logo_holder_large_logo_large_eng"   # .jpg + _.png (alpha)
OUT = os.path.join(ROOT, "packaging", "macos", "dmg") + "/"

S = 2                                   # drawn at 2x (Retina), scaled down for the 1x copy
W, H = 660 * S, 420 * S                 # the window (make_dmg.sh: --window-size)
APP_X, APPS_X, ICON_Y = 175, 485, 235   # icon centres at 1x (make_dmg.sh: --icon, --app-drop-link)


def shadowed(img, xy, text, font, fill, blur=3):
    s = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(s).text((xy[0] + 2 * S, xy[1] + 3 * S), text, font=font, fill=(0, 0, 0, 220), anchor="mm")
    img.alpha_composite(s.filter(ImageFilter.GaussianBlur(blur * S)))
    ImageDraw.Draw(img).text(xy, text, font=font, fill=fill, anchor="mm")


def main():
        # the world map without its parchment frame: the snow and sky at the top, the green
        # land and the rivers below
    m = Image.open(A + "A2Static/2Optimized/images/WorldMap/world_map_bg.jpg").convert("RGB")
    m = m.crop((250, 104, 1376, 821))
    bg = m.resize((W, int(W * m.height / m.width)), Image.LANCZOS).crop((0, 0, W, H)).convert("RGBA")
    bg = ImageEnhance.Brightness(bg).enhance(0.9)

        # soft shade at the top so the logo stands out from the snow
    shade = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shade)
    for y in range(H):
        t = max(0.0, 1 - y / (170 * S))
        sd.line([(0, y), (W, y)], fill=(18, 40, 78, int(150 * t)))
    bg.alpha_composite(shade)

        # the logo, a soft blue glow behind it
    logo = Image.open(LOGO + ".jpg").convert("RGBA")
    logo.putalpha(Image.open(LOGO + "_.png").convert("L"))
    lh = 140 * S
    logo = logo.resize((int(logo.width * lh / logo.height), lh), Image.LANCZOS)
    lx, ly = (W - logo.width) // 2, 10 * S
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((lx - 10 * S, ly, lx + logo.width + 10 * S, ly + lh), fill=(140, 220, 255, 90))
    bg.alpha_composite(glow.filter(ImageFilter.GaussianBlur(28 * S)))
    bg.alpha_composite(logo, (lx, ly))

        # the mist the icons' names sit on
    band = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    bd = ImageDraw.Draw(band)
    for y in range(H):
        t = (y / S - 262) / 46
        bd.line([(0, y), (W, y)], fill=(104, 122, 146, int(215 * (0 if t < 0 else min(1, t)))))
    bg.alpha_composite(band)

        # soft light behind each icon, and the arrow between them
    halo = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    hd = ImageDraw.Draw(halo)
    for x in (APP_X, APPS_X):
        hd.ellipse(((x - 70) * S, (ICON_Y - 66) * S, (x + 70) * S, (ICON_Y + 66) * S), fill=(255, 225, 150, 46))
    bg.alpha_composite(halo.filter(ImageFilter.GaussianBlur(18 * S)))
    arrow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ad = ImageDraw.Draw(arrow)
    x0, x1, y = (APP_X + 78) * S, (APPS_X - 80) * S, ICON_Y * S
    gold = (242, 214, 120, 255)
    for i in range(0, x1 - 26 * S - x0, 16 * S):
        ad.rounded_rectangle((x0 + i, y - 3 * S, x0 + i + 9 * S, y + 3 * S), 3 * S, fill=gold)
    ad.polygon([(x1, y), (x1 - 24 * S, y - 15 * S), (x1 - 24 * S, y + 15 * S)], fill=gold)
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    shadow.putalpha(arrow.getchannel("A"))
    bg.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(3 * S)), (2 * S, 3 * S))
    bg.alpha_composite(arrow)
    font = A + "fonts/ARICYRB.TTF"
    shadowed(bg, ((APP_X + APPS_X) // 2 * S, (ICON_Y - 30) * S), "DRAG TO INSTALL",
             ImageFont.truetype(font, 13 * S), (236, 236, 228))
    shadowed(bg, (W // 2, 384 * S), "Rule the Kingdom by Game Insight and innoWate   -   An UncleAcid1 project",
             ImageFont.truetype(font, 10 * S), (220, 224, 232), blur=2)

    os.makedirs(OUT, exist_ok=True)
    bg.convert("RGB").save(OUT + "background@2x.png", optimize=True)
    bg.convert("RGB").resize((W // S, H // S), Image.LANCZOS).save(OUT + "background.png", optimize=True)
    print("wrote", OUT + "background.png", "and background@2x.png")


if __name__ == "__main__":
    main()
