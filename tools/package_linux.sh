#!/bin/bash
# package_linux.sh BUILD OUT: dist/Rule-the-Kingdom-<version>-Linux-x86_64.tar.gz from the rtk built in
# BUILD: the program, the libraries that are not on every Linux (SDL3, pugixml, libpng, libjpeg) in
# lib/ (rtk's rpath is $ORIGIN/lib), the update packs in data/, and HOW_TO_PLAY.txt.
set -e
cd "$(dirname "$0")/.."
BUILD="$1"; OUT="$2"
VERSION="$(sed -n 's/^## Rule the Kingdom \([0-9][0-9.]*\).*/\1/p' CHANGELOG.md | head -1)"
NAME="Rule-the-Kingdom-${VERSION:-dev}-Linux-x86_64"
D="$OUT/$NAME"
rm -rf "$D"; mkdir -p "$D/lib" "$D/data"
cp "$BUILD/rtk" "$D/rtk"
cp gamedata/*.kbf "$D/data/"
cp packaging/HOW_TO_PLAY.txt "$D/"
ldd "$D/rtk" | awk '/=> \// {print $3}' | grep -E 'libSDL3|libpugixml|libpng|libjpeg|libvorbis|libogg' |
    while read -r lib; do cp -L "$lib" "$D/lib/"; done
ls "$D/lib"
tar -C "$OUT" -czf "$OUT/$NAME.tar.gz" "$NAME"
echo "$OUT/$NAME.tar.gz"
