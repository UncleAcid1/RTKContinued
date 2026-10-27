#!/bin/bash
# package_windows.sh BUILD OUT (in an MSYS2 CLANG64 shell): dist/Rule-the-Kingdom-<version>-Windows-x86_64.zip
# from the rtk.exe built in BUILD: the program, the DLLs it needs from MSYS2, the update packs in
# data/, and HOW_TO_PLAY.txt.
set -e
cd "$(dirname "$0")/.."
BUILD="$1"; OUT="$2"
VERSION="$(sed -n 's/^## Rule the Kingdom \([0-9][0-9.]*\).*/\1/p' CHANGELOG.md | head -1)"
NAME="Rule-the-Kingdom-${VERSION:-dev}-Windows-x86_64"
D="$OUT/$NAME"
rm -rf "$D"; mkdir -p "$D/data"
cp "$BUILD/rtk.exe" "$D/rtk.exe"
cp gamedata/*.kbf "$D/data/"
cp packaging/HOW_TO_PLAY.txt "$D/"
# the DLLs from MSYS2 (and the SDL3 built beside it), not Windows' own
ldd "$D/rtk.exe" | awk '/=> \// {print $3}' | grep -iv '/c/windows/' | while read -r dll; do cp "$dll" "$D/"; done
ls "$D"
(cd "$OUT" && rm -f "$NAME.zip" && zip -qr "$NAME.zip" "$NAME")
echo "$OUT/$NAME.zip"
