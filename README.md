# RTKContinued

A native, open-source C++ port of **Rule the Kingdom** (Game Insight, 2012–2014), rebuilt function by
function from the Android 5.11 game library so it can keep running on modern systems. Offline-first;
macOS (Apple Silicon) first, other platforms later. Built with SDL3 and OpenGL.

**Status:** work in progress. The city map, HUD, workers, the economy, saves and building placement
work. The shop is next. See [STATUS.md](STATUS.md) for the detailed progress and plan.

**Game data is not included.** You need your own copy of the original game files (the 5.11 Android
APK assets and the `.kbf` expansion packs). This repository contains only the port's source code and
the tools used to read the original formats.

## Building (macOS)
Requirements: CMake 3.20+, a C++17 compiler, and SDL3, pugixml, libpng, libjpeg and zlib
(e.g. `brew install cmake sdl3 pugixml libpng jpeg-turbo zlib`). FreeType 2.4.5 and SDL_ttf 2.0.11
are bundled in `third_party/`.

```
cmake -S . -B build
cmake --build build -j8
./build/rtk --root <folder containing the game data>
```

## Layout
- `src/` the port (engine, game logic, GUI, HUD, windows)
- `tools/` Python/Ghidra tools for the original's formats and for reading the decompile
- `docs/` working notes
- `third_party/` bundled libraries

## Reverse-engineering workspace

Code base: Android 5.11 `libkingdom.so` (ARMv7, Oct 2014, about 10.5k exported C++ symbols).
Primary data: the 5.11 APK `assets/data`. Secondary: the Win8 5.0.0.39 Appx and the `.kbf` expansion packs.

## Layout
- `tools/rtk_extract.py`: extracts the resource containers (Android `.jet`, Win8 `res_data.bin`, `.kbf`).
- `tools/ghidra_headless.sh`: runs Ghidra headless with the Homebrew JDK 21.
- `tools/ghidra_scripts/ExportDecomp.java`: dumps decompiled C for every function.
- `ghidra/`: the Ghidra project (`rtk511`), which can be opened in the GUI with `ghidraRun`.
- `out/`: generated output (assets, `decomp.c`, `functions.tsv`). It can be regenerated at any time.

## Regenerate
```
python3 tools/rtk_extract.py android "../Android files/rule-the-kingdom-5-11-multi-android/assets/data" out/assets/android511
python3 tools/rtk_extract.py win8    ../_extract/win5/Assets                            out/assets/win8
python3 tools/rtk_extract.py kbf     ../files/main.27.kbf                               out/assets/kbf_main27
tools/ghidra_headless.sh "$PWD/ghidra" rtk511 -import "../Android files/rule-the-kingdom-5-11-multi-android/lib/armeabi-v7a/libkingdom.so" \
    -scriptPath "$PWD/tools/ghidra_scripts" -postScript ExportDecomp.java "$PWD/out"
```

## Container format (verified on every entry)
Index entry: `u32 name_len` (includes the NUL), then the name, then 8 little-endian `u32` fields:
`hash, width, height, is_data, bpp, offset, size, chunk`.
- Android: payload is in `data/<chunk>.jet`. `offset` is global, so subtract the smallest offset of any entry in that chunk.
- Win8: payload is at `res_data.bin[offset:offset+size]`.
- KBF: header `u32 magic=0x75B4A351, u32 index_size, 3×u32`, then the index, then data at `20 + index_size + offset`.
- XML, xmlb, map `.bin` and TTF payloads are gzipped on Android (and some on Win8).
