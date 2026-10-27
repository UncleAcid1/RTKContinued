#!/bin/bash
# Makes the Mac disk image (DMG) for Rule the Kingdom from the code at HEAD:
#
#   1. checks out HEAD in a clean worktree (so local, uncommitted changes never get into a release)
#   2. builds the game there (a Release build) and its app (CMake target "app", tools/make_app.sh)
#   3. makes dist/Rule-the-Kingdom-<version>-macOS-<chip>.dmg: a window with the game's icon, an
#      arrow and the Applications folder over the original game's world map, and the game's icon on
#      the disk itself
#
# Run it from Terminal (from anywhere):   bash tools/make_dmg.sh
#
# The first run installs what it needs with Homebrew (cmake, sdl3, pugixml, libpng, jpeg-turbo,
# libvorbis, libogg, create-dmg). macOS asks once whether Terminal may control Finder: say OK,
# that's how the window gets laid out.
#
# The background is packaging/macos/dmg/background.png (+ @2x), drawn by
# tools/make_dmg_background.py; the icon positions below must match it. The DMG holds no game
# files: players need their own copy of the game (README "Play it").

set -e
cd "$(dirname "$0")/.."
ROOT="$(pwd)"
APP_NAME="Rule the Kingdom"
DIST="$ROOT/dist"

say() { printf '\n\033[1;33m==> %s\033[0m\n' "$1"; }
fail() { printf '\n\033[1;31m%s\033[0m\n' "$1" >&2; exit 1; }

[ "$(uname)" = "Darwin" ] || fail "DMGs can only be made on a Mac."
[ -d .git ] || fail "This folder isn't a git checkout (get the code with git clone)."

# ---------------------------------------------------------------------------------------------
say "Checking the tools"
if ! command -v brew >/dev/null; then
    fail "Homebrew is needed: install it from https://brew.sh, then run this again."
fi
for pkg in cmake sdl3 pugixml libpng jpeg-turbo libvorbis libogg create-dmg; do
    if ! brew list --versions "$pkg" >/dev/null 2>&1; then
        echo "Installing $pkg..."
        brew install "$pkg"
    fi
done
xcode-select -p >/dev/null 2>&1 || fail "Apple's command line tools are needed: run  xcode-select --install  then this again."

# ---------------------------------------------------------------------------------------------
say "Building the game from $(git log -1 --format='%h %s')"
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    echo "(This folder has changes of its own; they are left out: the build uses HEAD as committed.)"
fi
WORK="$(mktemp -d)"
trap 'git -C "$ROOT" worktree remove --force "$WORK/src" >/dev/null 2>&1; rm -rf "$WORK"' EXIT
git worktree add --detach "$WORK/src" HEAD >/dev/null
BUILD="$WORK/src/build"
cmake -S "$WORK/src" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD" -j "$(sysctl -n hw.ncpu)"
cmake --build "$BUILD" --target app
APP="$BUILD/$APP_NAME.app"
[ -d "$APP" ] || fail "The build didn't make $APP_NAME.app."

# ---------------------------------------------------------------------------------------------
say "Making the disk image"
VERSION="$(sed -n 's/^## Rule the Kingdom \([0-9][0-9.]*\).*/\1/p' CHANGELOG.md 2>/dev/null | head -1)"
[ -n "$VERSION" ] || VERSION="dev"
if [ "$VERSION" != "dev" ]; then
        # builds made after the release's own commit say which commit they're from
        # (only what goes into the app counts: a change to this script leaves it the release)
    if ! git diff --quiet "$(git log -1 --format=%H -- CHANGELOG.md)" HEAD -- src third_party \
            CMakeLists.txt packaging/macos/Info.plist packaging/macos/AppIcon.icns tools/make_app.sh 2>/dev/null; then
        VERSION="$VERSION+$(git rev-parse --short HEAD)"
    fi
fi
CHIP="$(uname -m)"
[ "$CHIP" = "arm64" ] && CHIP="AppleSilicon" || CHIP="Intel"
DMG="$DIST/Rule-the-Kingdom-$VERSION-macOS-$CHIP.dmg"

STAGE="$WORK/dmg-staging"
rm -rf "$STAGE"
mkdir -p "$STAGE" "$DIST"
ditto "$APP" "$STAGE/$APP_NAME.app"
    # one background with both sizes, so Retina screens get the sharp one
tiffutil -cathidpicheck packaging/macos/dmg/background.png packaging/macos/dmg/background@2x.png \
    -out "$WORK/dmg-background.tiff" >/dev/null 2>&1 || cp packaging/macos/dmg/background.png "$WORK/dmg-background.tiff"
rm -f "$DMG"

# Icon centres: the game at (175, 235), Applications at (485, 235), in a 660x420 window
# (tools/make_dmg_background.py draws the arrow between them).
# Finder sometimes refuses the window settings ("Can't set statusbar visible", -10006): it's
# tried three times, then the image is made without the arranged window rather than not at all.
make_dmg() {
    rm -f "$DMG" "$DIST"/rw.*.dmg
    create-dmg "$@" "$DMG" "$STAGE"
}
DMG_LOOK=(--volicon "$APP/Contents/Resources/AppIcon.icns"
        --background "$WORK/dmg-background.tiff"
        --window-pos 200 120
        --window-size 660 420
        --icon-size 112
        --text-size 13
        --icon "$APP_NAME.app" 175 235
        --hide-extension "$APP_NAME.app"
        --app-drop-link 485 235)
made=no
for try in 1 2 3; do
    if make_dmg --volname "$APP_NAME" "${DMG_LOOK[@]}" --hdiutil-quiet; then
        made=yes
        break
    fi
    echo "(Finder didn't arrange the window; trying again, $try of 3)"
    sleep 3
done
if [ "$made" = no ]; then
    echo "Making it without the arranged window instead (the game inside is the same)."
    if make_dmg --volname "$APP_NAME" --skip-jenkins --app-drop-link 485 235 --hdiutil-quiet; then
        made=yes
    fi
fi
if [ "$made" = no ]; then
    echo
    echo "If it says Terminal isn't allowed to control Finder: open System Settings > Privacy &"
    echo "Security > Automation, turn on Finder under Terminal, and run this again."
    fail "The disk image wasn't made."
fi

say "Done"
echo "$DMG ($(du -h "$DMG" | cut -f1))"
open -R "$DMG"
