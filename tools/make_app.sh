#!/bin/sh
# make_app.sh BINARY OUTDIR: builds "OUTDIR/Rule the Kingdom.app" from the rtk binary (CMake target
# "app": cmake --build build --target app). The bundle has the bundle id the system notifications
# need (packaging/macos/Info.plist), the icon (packaging/macos/AppIcon.icns, tools/make_app_icon.py),
# the Homebrew libraries the binary links (copied to Contents/Frameworks, their install names pointed
# there), the game's update packs (gamedata/*.kbf in Contents/Resources/data) and an ad-hoc signature.
# The player's 5.11 APK is not in it: the game looks for it beside the app (main.cpp FindGameApk).
set -e
BIN="$1"; OUT="$2"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
APP="$OUT/Rule the Kingdom.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/Resources"
cp "$HERE/packaging/macos/Info.plist" "$APP/Contents/Info.plist"
cp "$HERE/packaging/macos/AppIcon.icns" "$APP/Contents/Resources/AppIcon.icns"
mkdir -p "$APP/Contents/Resources/data"
cp "$HERE"/gamedata/*.kbf "$APP/Contents/Resources/data/"   # the packs that ship with the game (main.cpp FindGameData)
cp "$BIN" "$APP/Contents/MacOS/rtk"
# Non-system libraries, followed through the copied libraries' own links.
fix() {
    otool -L "$1" | tail -n +2 | awk '{print $1}' | grep -v '^/usr/lib/\|^/System/\|^@' | while read -r dep; do
        name="$(basename "$dep")"
        if [ ! -f "$APP/Contents/Frameworks/$name" ]; then
            cp "$dep" "$APP/Contents/Frameworks/$name"
            chmod u+w "$APP/Contents/Frameworks/$name"
            install_name_tool -id "@rpath/$name" "$APP/Contents/Frameworks/$name" 2>/dev/null
            fix "$APP/Contents/Frameworks/$name"
        fi
        install_name_tool -change "$dep" "@rpath/$name" "$1" 2>/dev/null
    done
}
fix "$APP/Contents/MacOS/rtk"
install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP/Contents/MacOS/rtk" 2>/dev/null || true
codesign --force --deep --sign - "$APP" 2>/dev/null
if otool -L "$APP/Contents/MacOS/rtk" "$APP"/Contents/Frameworks/* | grep -q '/opt/\|/usr/local/'; then
    echo "make_app.sh: a library still points outside the bundle" >&2; exit 1
fi
echo "$APP"
