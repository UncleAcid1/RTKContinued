#!/usr/bin/env python3
"""Makes the app icon (packaging/macos/AppIcon.icns) from the original 5.11 Android icon
(res/drawable-xxhdpi/icon.png, 144x144, the largest the game has), scaled to every size macOS asks for.

Usage: tools/make_app_icon.py        (needs: pip install pillow; macOS for iconutil)
"""
import os, subprocess, tempfile
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = os.path.join(ROOT, "..", "Android files", "rule-the-kingdom-5-11-multi-android", "res", "drawable-xxhdpi",
                   "icon.png")
OUT = os.path.join(ROOT, "packaging", "macos", "AppIcon.icns")


def main():
    icon = Image.open(SRC).convert("RGBA")
    with tempfile.TemporaryDirectory() as tmp:
        iconset = os.path.join(tmp, "AppIcon.iconset")
        os.makedirs(iconset)
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                px = size * scale
                name = f"icon_{size}x{size}{'@2x' if scale == 2 else ''}.png"
                icon.resize((px, px), Image.LANCZOS).save(os.path.join(iconset, name))
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", OUT], check=True)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
