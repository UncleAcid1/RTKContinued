# Port status and handoff

This file is the single source of truth for where the project stands. Update it at every milestone.

## Goal
A native, open-source C++ port of Rule the Kingdom (Game Insight, 2012–2014). It has to match the original
game's code exactly, run on macOS (Apple Silicon) first and then any OS, and be developable going forward.
Offline-first: online-only features (PvP, friends, Facebook, purchases) are stubbed for now and may be
reworked to work offline later. Rebuilding the server is a separate future project.

## Source of truth
- Code: `../Android files/rule-the-kingdom-5-11-multi-android.apk`, `lib/armeabi-v7a/libkingdom.so`
  (Oct 2014, ARMv7). It has about 10.5k exported C++ symbols. The decompile lives in `out/decomp.c`
  (regenerate with the README).
- Assets, layered: 5.11 `assets/data` → `../files/main.27.kbf` → `../files/patch.33.kbf` → Win8 Appx
  `../WINDOWS/0EB8BD08.RuletheKingdom_5.0.0.39_x86__erk4rrwmt7jyt.Appx` (fallback).

## Accuracy rules (non-negotiable)
1. Every ported function carries a `// @0xADDR Class::Method` comment pointing at the original.
2. Formats and formulas come from the decompile or assembly, never from guessing at bytes.
3. When the decompile shows `extraout_sN`, read the assembly. The library is **softfp** (floats are passed
   in core registers), which the decompiler mishandles. Use `tools/ghidra_scripts/DumpAsm.java` +
   `tools/pic_strings.py`.
4. Anything not yet verified gets marked `// UNVERIFIED:` with the reason.

## Phase 1: formats (DONE)
| Format | Tool | Verified |
|---|---|---|
| res_desc/.jet, Win8 res_data, .kbf | `tools/rtk_extract.py` | every entry, in all 4 sources |
| maps (old + chunk format) | `tools/rtk_map.py` | all referenced maps byte-exact; 14 dead 2012 files are unused |
| .xmlb UI layouts | `tools/rtk_xmlb.py` | 304/304 internally consistent |
| static map render | `tools/render_map.py` | map_0 matches the reference screenshot |

## Phase 2: engine (IN PROGRESS)
Stack: C++17, CMake, SDL3, OpenGL (the game's own GLSL), pugixml, libpng/libjpeg, zlib, FreeType.

Milestones:
1. [x] Native window showing map_0 live with pan/zoom, rendered by ported C++
       (FileManager → Resources → Render → Map::Load → Background/Decor/Building sprites).
       Verified 2026-10-04: pixel diff vs tools/render_map.py = 0.48% of pixels (edge filtering only).
       Build: `cmake -S . -B build && cmake --build build -j8`; run: `./build/rtk --root ..`
       (drag to pan, wheel to zoom, F12 screenshot, Esc quit; `--screenshot f.png --camera X Y Z` headless).
2. [ ] GUI from .xmlb (HUD, windows, text). IN PROGRESS, sub-steps:
       2a [x] fonts (FreeType 2.4.5 + SDL_ttf 2.0.11, the .so's versions) + GUI core + xmlb loader
       2b [x] text/glow + StringTable
       2c [x] HUD windows: HUDWindow (frame border, locators, farm timer, map-name popups), TopCity
              (resource bar, scrolls via Shared::ContentScroller), PlayerTop, CastleTop, BattleBar,
              BeltBar, BottomCity (Build/Tools + instruments), TaskHolder (queue slot only).
       2d [~] input: Window::Click, Textfield::Click, WindowManager queues (ProcessClick head-first,
              ProcessUpdate/ProcessMove tail-first, DesktopWindow at the bottom), press highlight,
              tap ring, mouse history + GetMouseSpeed, interaction locks, ContentScroller (drag,
              fling, snap, bounce, scrollbar). Verified by headless screenshots.
       Remaining for M2: dialogs/windows beyond the HUD (shop, exchange, ...), text input
       (BeginTextInput), MapMovement (main.cpp has a stand-in pan), sounds.
       Deferred to later milestones (marked UNVERIFIED in the code): BuildingMovement/Placement and
       BuildingHovers hooks in the panels' Click, ShopWindow, quest UI (TaskHolder Init/SetZ/Update,
       milestone 4), Map::GetTotalStorageLimit (resource limits, milestone 3), texture frame-chain
       animation (IconManager sand clock / exclamation), HP-restore and hint hover windows.
       Headless testing: `./build/rtk --screenshot f.png [--click X Y]... [--press X Y]` runs 60
       frames, then each click (press, 3 frames, release, 30 frames) in framebuffer pixels. Don't run
       rtk without --screenshot from a tool call: it opens the window and blocks.
3. [ ] Game data + GameState + save/load; building and economy loops.
4. [ ] Entities/AI/pathing, quests (Tasks), combat, campaign maps.

## Open questions (tracked)
- RESOLVED: shader type 2 is mainVS+mainPS (Render::InitMain); 5.11 has no terrain shader at all.
- Building+0xCC (input to GetSpriteZ for building parts) has an unknown source.
- `GameState::playerSeed` initialisation for new games (FUN_00193d44) hasn't been read.
- 47 UI images aren't in any source; most are online-only screens.

- Render: textures are premultiplied (c*a/255.0, truncated); blend ONE, ONE_MINUS_SRC_ALPHA; no depth test.
- Only layers passed to SortRenderLayer are sorted (layer 8 with SpriteSortZ); std::sort = STLport introsort (ported).
- On map 0, Building::LinkBaseToBuilding removes whole decorations under building footprints at load.

## Key facts (see the tools for details)
- Ghidra image base 0x10000: file vaddr = Ghidra address − 0x10000.
- Projection: worldX = x·84 + (y odd ? 42 : 0), worldY = y·21. A sprite position is its bottom-left corner.
- Layers: 0 ground + horizon, 1 dark grass, 2/3 flat decor + flag posts, 7 rings, 8 objects (sorted by Z
  with SpriteSortZ: higher z first), 11 overlays, 14 GUI.

## Milestone 2 notes (verified from softfp decompile/asm)
- Decompile: `tools/ghidra_softfp_patch.py` makes Ghidra's ARM default prototype softfp, so float args
  show up. `out/gui.c` = GUI/Render/HUD/StringTable functions with it (`tools/fn.sh 'regex' out/gui.c`).
  Full softfp `out/decomp.c` exported 2026-10-04 (12,924 functions, 7 failed); the old hard-float
  export is `out/decomp_hardfp.c`. `tools/asm.sh 'regex'` / `tools/asmr.sh` read out/asm_all.txt;
  `tools/pic_data.py` annotates PIC data reads (not .bss bases: add `ldr lit` + pc by hand);
  `tools/vtable.py <GOT value + 8>` prints a vtable; `tools/elfread.py` reads the .so (rd(addr, n)).
- Screen (SDL_baseInit @0x18b1e0): W/H globals 0x60ef6c/0x60ef70. max<600 small screen; max>=1850 or
  min>=1000 -> HighDPI flag (0x612424)=1, BaseZoom 1.5, GUI HighDPI 2.0, hover 2.0, HUD 1.6, ForceLinear.
  Defaults HighDPI/hover/hud = 1.25. Tablet = min>=600. Port plan: device screen = framebuffer pixels.
- GetScaleFactor(w,h,_,s): s=HighDPI?hdpi:s; if s*h>H s=H/h; if w*s>W s=W/w.
- Window layout (0x80): see src/gui/GUI.h. UpdatePosition: x=(int)(rootX+x+cx)-0.25,
  y=(int)(rootY+y-cy)+h-0.25, z=window z; cx/cy=(size-spriteSize)/2 if centered. SetZ: children take
  root.zNext -= 0.0001. Shader: disabled 3 (text/grayscale), alpha==1 0 (main) else 1 (text).
- Update9Slices: 9 sprites chained via sprite+0x64, pos +0.5, order L-mid,R-mid,T-mid,B-mid,TL,TR,BL,BR,
  center (skipped if +0x5f); insets scaled by root scale, shrunk if w<l+r.
- Layer::Prepare: vertices V0(x,-y,u0,vB,a0) V1(x+w,-y,u1,vB,a1) V2(x,h-y,u0,vT,a2) V3=V1 V4=V2
  V5(x+w,h-y,u1,vT,a3); corner alphas sprite+0x44/48/4c/50; screenSpace adds 0.25 to x,y.
- xmlb: image idx (1-based) == own index -> load "<dir>/<RootSymbol>___<name . -> _>.png", else share
  windows[idx-1] texture. Font size=(int)(size*scale*fontScale*global(1.0)); colors /255.
- CreateTextInternal: parts split at \n, literal \\n, spaces (wrap), <b><i><u><s> tags (bits 1,2,4,8);
  SDL_Color passed as {r=B,g=G,b=R} so surface bytes are RGBA; lines y += spacing+lineSkip; align
  0/1/2/3(justify); glow via ApplyImageFilters (gaussian, radius=(int)(blur*overscale)<=32, sigma=r/5
  min 0.9) — transcribe from out/gui.c. Text sprite: u 0..1, vB 0 vT 1, h = -texH, shader 1.
- HUD = HUDWindow::Show: BattleBar, BeltBar, PlayerTop, TopCity, CastleTop, BottomCity, TaskHolder
  (FunctionalWindow statics in a global WindowQueue list; z via MoveWindowOnTop).
