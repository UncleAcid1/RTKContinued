# Port status and handoff

This file is the single source of truth for where the project stands. Update it at every milestone.

## Handoff (last updated 2026-10-06, 3e done; next 3f) — read this first in a new conversation
**Where things stand.** Milestones 1–2 done; milestone 3 (city economy) steps 3a–3e done, 3f (farms) left:
3a/3b city systems, 3c workers/economy/hover windows (tax, build bubbles, regrow and build progress
boxes, drops), 3d saves (see the 3d entry below).  GitHub: https://github.com/UncleAcid1/RTKContinued
(branch main, first pushed 2026-10-06). Push after a finished step only when the user asks.

**User's standing rules.** 100% faithful to the original (every function has `// @0xADDR`, guesses
marked UNVERIFIED/PORT); clean, no over-engineering; offline-first; confirm before downloads, installs
or anything outward-facing; never share the user's email; base-game parity first; keep this file as
the handoff; give the user a short update after each finished step (3e, 3f, ...).

**Next work, in order.**
0. 3e.1 DONE (c8494a3): Map::AssignEntities (residents/workers back to homes and jobs; decoration
   jobs and the farm-farmer pass UNVERIFIED), Building::UpdateOfflineState/UpdateOfflineResources,
   Map::UpdateOfflineResources, GetBuildingWithID. Verified: a worker saved on a tree reloads
   working it; a resident reloads at the castle. RefreshOfflineGoblins (class 0x10) left for M4.
   3e.2 DONE: BuildingPlacement (src/game/BuildingPlacement.*: BuildingBought/DecorBought, the
   free-spot search, drag/tap/edge-scroll, rotate, Accept/Decline, red/green footprint),
   PlaceBuildingHoverWindow (hud/HoverWindows), HiddenObjects, the helper arrow (BuildingHovers
   ArrowAt/HideArrow/UpdateArrow), Map grid/patch/road helpers (GetPatchForCoordinates,
   IsValidAreaForBuildZone, GetHQ, virtual decorations, UpdateRoadConnections + RoadData table),
   Building/Decor CanBePlaced, Building::Duplicate, GameState::RemoveItem/latestUniqueID,
   HUDWindow::SetInfoText/SetBottomType, NotEnoughWindow requirements (dialog: 3e.4), ShopWindow
   stand-ins (src/windows/ShopWindow.cpp: never open; 3e.3 replaces them). Fixed on the way: AI
   waypoint marker starts at 1 (search found nothing before), LinkBaseToBuilding uses the
   mirror-aware start tile. Verified headless (a temporary RTK_BUY hook in main, removed): buy,
   free spot, drag onto trees -> red + confirm disabled, tap/drag to grass, confirm charges the
   costs, builds a site with unique id, saves and reloads with its build bubble; rotate; cancel
   restores everything. Known until 3e.3: the shop's Building instance (opened? level?) and the
   icon flight (AddItemMovement) are unverified; after Cancel the arrow stays because the
   original hides it when the reopened shop counts as a shown window.
   3e.3 DONE: ShopWindow (src/windows/ShopWindow.cpp; decode in docs/3e3_shop_notes.md): tabs
   from shoptabs.xml, cells with lock/status/crystal looks, info panel (purpose lines, cost lines,
   level/building requirements), ContentScroller strip, slide in/out, tap-to-buy into
   BuildingPlacement (building and decoration), NotEnough path, people-limit popup, tutorial 0x5b
   arrow; BottomCityWindow::OnBuild wired. Also GUI::FitImageIntoWindow, BuildingHovers::
   AddItemMovement (the icon flight), SetArrowClickCallback/ClickOnArrow/SetArrowVisibleWindowLimit,
   Map::Decor::GetData (lazy data lookup; a bought decoration has only its id). Verified headless:
   Build opens the shop, tabs switch, a flowerbed buys/flies/places (confirm charges 15 gold,
   cancel reopens the shop), a building buys (costs charged, pending worker 1/2), unaffordable ->
   NotEnough requirements, the close button. Open: Tasks (quest-locked items, M4), Items in
   producing lines, the "%s$%$d" cost line (BuildingPlacement::UpdateCost), sounds.
   3e.4 PART 1 DONE (decode in docs/3e4_notenough_notes.md): ConfirmPurchaseWindow
   (src/windows/ConfirmPurchaseWindow.cpp; skipped, buying at once, unless secondTutorial == 0x100
   - the PORT test city is 0x81, so it buys without asking there), the NotEnoughWindow dialog (Show
   with the single-missing popups/exchange, UpdateContents lines and layout, Buy all, Find with
   OnFindActual/FindResourceHelp, the requirement setters), the hover speed-ups
   (BuildProgress/ResourceRestore OnSpeedUp, BuildProgress OnSpeedUpFinished with its tutorial
   steps), Building::SpeedupBuilding, Decor::SpeedupDecoration, Map::GetUnfinishedBuildingWithID /
   GetUnfinishedBuildingedWithPopulation / GetUpgradeableBuildingWithID, GameState::
   IsBossCombatActive (false until M4), ShopWindow::OnTabSelect and TopCityWindow::Click made public.
   Verified headless: an unaffordable Wattle Shack opens the dialog (4 lines, Find, Buy all 8);
   Find Wood closes it and points the arrow at the tree; Buy all with 50 crystals tops each
   resource up to exactly what is needed, charges 8 and starts the placement; a construction
   site's Speed Up (5) opens the confirm dialog (with secondTutorial forced to 0x100), Okay
   finishes the building and charges 5, Cancel charges nothing; with 2 crystals the single
   missing requirement goes to the exchange ("short by 3"). Not tested headless: the tree/rock
   speed-up (SpeedupBuilding class 4) and ResourceRestoreHoverWindow (a spawned worker would not
   start gathering in the test runs).
   Stand-ins (UNVERIFIED): the building info windows (BuildingHovers::OnBuildingClick,
   ShowArrowAtUpgrade) for the Find arrows' callbacks; the animated CenterOn at zoom 0.4; the
   upgradable-building purpose block (with the building upgrade window); items/professions (M4).
   3e.4 PART 2 DONE: ExchangeWindow (src/windows/ExchangeWindow.cpp; Payment_screen.xml, tabs,
   6 packs with the original amounts/bonuses/banners, header text, refresh each second). PORT
   (user decision, see Freemium removal): the real-money store became a two-way exchange - a
   gold pack costs crystals (confirmed with ConfirmPurchaseWindow), a crystal pack costs gold at a
   worse rate; rates in the settings port_exchange_gold_per_crystal (200) and
   port_exchange_gold_per_bought_crystal (400) set in main.cpp, placeholders to tune. After a
   trade: the original "You've received" popup and Map::SafeSave (so test runs need a fresh
   --storage). The HUD's Buy button (TopCityWindow::OnCB) opens it. LevelUpWindow
   (src/windows/LevelUpWindow.cpp): level, greeting, the unlock strip (crystal reward, the shop's
   buildings of that level; Tasks' "new tasks" cell is M4) with arrows/scrolling and centring,
   level_up_cb crystals paid on close (dropped at the hero's feet once M4 has a hero), saved on
   open; shown from GameState::ChangeResourceAmount on a level-up and deferred through the new
   WindowManager::EnqueueWindow/Update/ClearQueue while another window or a placement is up. PORT:
   the Facebook/Twitter share is not offered (plain "Ok"). Also Shared::LargeLogoWindow.
   Verified headless: the exchange from NotEnough's Buy all and from the HUD; 6,000 gold -> 15
   crystals with the popup; 15 crystals -> 3,000 gold through the confirm dialog; too few
   crystals -> "short by 65 crystals" in the header, nothing charged. Level 2 (600 XP) shows the
   crystal reward and 3 buildings centred, Ok pays 2 crystals and the save is written; level 4
   (2,600 XP) has 5 cells, the right arrow scrolls to Enclosure; a level-up with the shop open
   waits and shows when the shop closes.
   Headless-test timing: after a shop purchase the icon flies for 1 s (BuildingPlacement::Update,
   as the original) before the placement controls slide in, and taps are ignored while a GUI
   animation runs (WindowManager::ProcessClick). The harness waits exactly 30 frames after a tap,
   so a Confirm tap right after the shop tap is lost; put a `--key 4` (30 more frames) before it.
   "Buttons_confirm_placement.xml z range exhausted" is printed because PlaceBuildingHoverWindow
   moves itself on top before it counts as visible (the original's order, so its z is the same).
   3e.5 DONE: land buying (fb1ec73): LandWindow (the area's objects "%dx %s", tree/rock counts,
   gold and crystal buttons, level lock; Map::ClickToBuyArea on a for-sale sign, Map::BuyArea,
   crystal path also Map::Save(0)) and LandExpandedWindow (PORT: no share; the empty share backdrop
   beside Ok is baked into the window's background image). The farm-patch half of both is 3f.
   BuildingMovement (src/game/BuildingMovement.*, the Tools button): actions with undo (move,
   remove, continuation), previews (copies) with red blocked tiles, Rotate (mirror), Remove (the
   "can't destroy the last" popup, the decoration question), Accept (arrows over blocked previews,
   else applies everything, Map::Save(0)) and Decline (undoes all). Wired in main.cpp (mouse
   down/move/up before BuildingPlacement), BottomCityWindow, PlaceBuildingHoverWindow (legacy
   mode), BuildingHovers (no hovers while editing), WindowManager::Update, LevelUpWindow,
   Game SaveOnExit and Map::SafeSave (Decline first). Added on the way: Map::RemoveBuilding/
   RemoveDecoration/SetDecoration, Building PrepareToAction/UndoAction/OnMoved/OnDestroy/CleanUp,
   GameState RemoveAllTargetOrders/RemoveOrderOfWorker, EntityManager::ResetOrders.
   Not ported (dead in 5.11, nothing calls their toggles): road painting (mode 3) and the
   warehouse (mode 4). PORT: decoration sprite visibility is set explicitly; Map::RemoveDecoration
   flags `removed`.
   Verified headless: move onto blocked tiles (red, Confirm shows "!"), move to grass + Confirm
   persists after reload, Cancel restores, Rotate mirrors on one tap, removing the last building
   of a kind is refused, a decoration removal asks and applies on Confirm. Random decorations
   (IsFake, regenerated from the player seed) grow back after a reload; the original's remove
   path does not check IsFake either, so this is faithful.
   NEXT: 3f farms.
   Tools: tools/picvar.py (after picsym.py) resolves `iVarN + 0x......`; tools/wstr.py prints
   UTF-32 (wchar_t) literals.
1. 3e shop/economy: ShopWindow, BuildingPlacement/BuildingMovement (Accept calls Map::Save(0)),
   costs, building limits, LandWindow area buying (also Map::Save(0)), LevelUpWindow (Map::Save(0)),
   NotEnoughWindow/ConfirmPurchase (makes the hover Speed Up buttons work: BuildProgress/
   ResourceRestore OnSpeedUp), ExchangeWindow, factory/farm hovers. Then the offline catch-up:
   Map::AssignEntities @0x1b99f0 (decompiled and read; residents loaded by GameState::LoadEntities
   are created but not yet placed in houses), Building::UpdateOfflineState, Decor::UpdateOfflineState,
   Map::UpdateOfflineResources (a stub comment in LoadSavedGame, src/game/Game.cpp). Hook point:
   the UNVERIFIED comment at the end of Map::Load.
2. 3f farms (see the 3f entry): next.
3. Milestone 4: hero/army/AI, quests (Tasks, MetaExpression), items (GameState keeps the item list and
   bindings already; Items::GetItemInfo missing), new-game tutorial on map 0x15 (replaces the PORT
   test city in main.cpp), spawns/portals/fog (their map chunks are passed through raw in
   Map.cpp g_otherChunks), the hero/soldier entity chunks (kept raw in GameState g_keptEntities),
   presents (chunk 8 raw).

**Freemium removal (user decisions, 2026-10-06). These override "100% faithful" for monetisation.**
- **No real-money purchases anywhere.** Remove every "buy" or "pay" mention for real money: the
  Exchange window's dollar packs, Billing, and offers.
- **Crystals (diamonds) come from play.** Small passive rewards from most quests, task-completion
  systems, the hunts/grinding system and the arena. They should add up over time to afford shop
  items.
  - The exact amounts are not decided yet; propose them when those systems are ported.
- **Spending crystals in-game stays:** speed-ups, Buy all, the crystal/gold exchange, and potions.
- **Exchange (decided 2026-10-06):** 5.11's ExchangeWindow only sold gold and crystal packs for real
  money. Offline it is a two-way exchange: crystals buy gold, gold buys crystals at a worse rate.
  Rates are placeholders, to tune so grinding players can afford everything. Future idea from
  the user: a diamond mine run by the treasury.
- **Admin/debug mode:** a permission that opens the store menu and grants the items for free, for
  testing throughout the game.
- **Health (future):** the hero restores health much faster after dying. Potions are bought with
  earned crystals.
- **Standing rule:** when a gameplay feature was built for the mobile/freemium model or needs
  online content (energy timers, paid speed-ups, friends, PvP servers, offers ...), ask the user
  how it should work offline before porting it.

**How to work.**
- Decompile: `tools/fn.sh 'regex' | python3 tools/picsym.py`; asm in `out/asm_all.txt`;
  `tools/elfread.py` rd/u32/cstr (Ghidra addresses). PIC globals: value = u32(DAT_lit) + constant
  shown by Ghidra (no +0x10000); GOT slots: `u32(picsym.GOT[0] + u32(DAT))`. A scratch resolver
  that rewrites `iVarN + 0x...` into addresses was useful (track `iVarN = DAT_...` assignments,
  including ones inside `for(...;iVar = DAT, ...)`); exported data names via `objdump -T --demangle`.
- Build: `cmake --build build`. Headless test: `./build/rtk --root .. --screenshot out.png
  [--storage DIR] [--save] [--click X Y] [--spawn ID X Y] [--frames N] [--camera X Y Z]
  [--dump-entities]`. Always pass `--storage <scratch dir>` in tests so the user's real save
  (~/Library/Application Support/Rule the Kingdom/save.bin) is untouched. `--save` runs
  Map::SafeSave before the shot. Save files are gzip; decode with a few lines of Python
  ([u32 count][u32 mapId][u32 size][chunks] big-endian; chunk = [type][size][data], ends 0x13).
- Temporary test hacks go in src/main.cpp; back it up first, restore before committing.
- Timer::GetGlobalTime is wall-clock seconds (+ saved time advance), so time-based states need
  real seconds or forced refreshes (BuildingHovers::Update(0, true)).

**3d facts worth knowing.** Save format: SaveManager (src/game/SaveManager.*), version 0x400,
blocks per map plus the game state (mapId 0xffffffff). GameState::Save/Load in src/game/GameState.cpp
mirror every chunk of the original (chunk names table in SaveManager.cpp). Map::Load now takes a
SaveBlock and a time (lrand48 seed); random decorations use GameState::playerSeed. Offline the
original never saves on a timer (only the "fb" profile does): it saves on exit, app pause and game
events. GameState::tutorial/secondTutorial are real now (Reset gives 0 / 0x81; the PORT test path
sets tutorial 0x100).

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
       (drag to pan, wheel to zoom, F12 screenshot, Esc quit; `--screenshot f.png --camera X Y Z` headless:
       view centred on world X,Y at Z times the default zoom, then clamped by ApplyViewportLimit).
2. [x] GUI from .xmlb (HUD, windows, text). Done 2026-10-05, sub-steps:
       2a [x] fonts (FreeType 2.4.5 + SDL_ttf 2.0.11, the .so's versions) + GUI core + xmlb loader
       2b [x] text/glow + StringTable
       2c [x] HUD windows: HUDWindow (frame border, locators, farm timer, map-name popups), TopCity
              (resource bar, scrolls via Shared::ContentScroller), PlayerTop, CastleTop, BattleBar,
              BeltBar, BottomCity (Build/Tools + instruments), TaskHolder (queue slot only).
       2d [x] input: Window::Click, Textfield::Click, WindowManager queues (ProcessClick head-first,
              ProcessUpdate/ProcessMove tail-first, DesktopWindow at the bottom), press highlight,
              tap ring, mouse history + GetMouseSpeed, interaction locks, ContentScroller (drag,
              fling, snap, bounce, scrollbar). Verified by headless screenshots.
       2e [x] camera + MapMovement: Render::offsetX/offsetY/zoom/aspect (the original globals),
              GetDefaultZoom, CenterOn (instant path), SetViewportMapBounds + ApplyViewportLimit (from
              Map::GetAreaBorders), wheel zoom, MapMovement drag/inertia, main_Loop_Func mouse dispatch
              (400 px jump filter, first-move-per-frame ProcessMove) and the game Update order.
              Verified headless: drag pans with fling, release over a button doesn't press it,
              clamp converges to the map edges/horizon when zoomed out.
       2f [x] dialogs + text input: GUI::AnimationEffect base (MovementEffect, TweenEffect: dialogs drop
              in from the top and fall out the bottom), PopupWindow (the message box: OK / OK+Cancel /
              close, grows with the text), SettingsWindow (from the navigation panel's gear: music,
              sound and notification toggles work; online buttons close it), CityRenameWindow,
              Shared::ButtonTripleInfo/SmallLogoWindow, GameState pause counter / IsCityTutorial /
              castle name, SystemFuncs settings (in memory), Timer::GetTime/GetGlobalTime.
              Text input: TextInput (the binary's keyboard path) + FileManager::BeginTextInput /
              EndTextInput / AbortTextInput + TextfieldCallback (cut to the field's maxLength = the
              byte SetEditable sets). PORT: Android used a Java text dialog; the Mac port types into
              the field, characters from SDL text events (any layout/IME), Backspace/Return by scancode.
              Keys (main_Loop_Func SDL_KEYDOWN): Menu/V opens settings (only at secondTutorial 0x100),
              Back/Y -> WindowQueue::ProcessBack, else hide popup / the MOBILE_EXIT_CONFIRM question
              (OK = ExitWithSendSave -> MainExit sets `done`). Fixed a 2d bug: a release on a root without
              onClick clears the pressed window of that layout (g_pressed), not the text-input field.
              Verified headless: settings toggles, X / outer click / back key close (screen returns
              pixel-identical), typing (12-char cut, Unicode), empty-name error popup, exit question.
       Moved out of M2: GUI/game sounds -> milestone 5 (every SoundsManager call site is marked
       UNVERIFIED); dialogs that show game data (shop, exchange, crafting, quests, character, level
       info, friends/PvP, hovers ...) -> milestones 3-4, with the systems they display.
       Deferred to later milestones (marked UNVERIFIED in the code): BuildingMovement/Placement and
       BuildingHovers hooks (tutorial arrows, SetArrowAlpha) in the panels' and dialogs' Click,
       ShopWindow, quest UI (TaskHolder Init/SetZ/Update, milestone 4), Map::GetTotalStorageLimit
       (resource limits, milestone 3), the CenterOn camera tween and Map camera points, the initial
       camera on the player entity (stand-in position), touch/pinch zoom, WindowQueue::ProcessWheel,
       the world click (EntityManager/buildings/ClickToBuyArea), Map::GetCurrentFarm (ProcessBack),
       texture frame-chain animation (IconManager sand clock / exclamation), HP-restore and hint hover
       windows, SystemFuncs settings persistence (milestone 6), GameState::secondTutorial (stand-in 0x81).
       Headless testing: `./build/rtk --screenshot f.png [--show city_rename] [--click X Y]...
       [--press X Y] [--drag X1 Y1 X2 Y2]... [--type TEXT]... [--key SCANCODE]...` runs 60 frames,
       then each input in order: a mouse input is press, 3 frames, [10 moves, one per frame,]
       release, 30 frames, in framebuffer pixels; --type/--key feed the keyboard path, then 30 frames
       (e.g. --key 28 = Y/back, 42 = Backspace, 40 = Return). `--show` is a PORT test aid that opens a
       window not reachable yet. A drag's fling uses real time, as on the original. Don't run
       rtk without --screenshot from a tool call: it opens the window and blocks.
3. [ ] Game data + GameState + save/load; building and economy loops. Sub-steps:
       3a [x] data foundation (2026-10-05): settings (game/Setting: dynamic_config <px> key=value
              trees via the Parse*Setting grammar, Setting handles, GetSetting/SetSetting, level XP
              table, <ar> areas -> GameState::AreaInfo), the main_Loop_Init hard-coded settings (flag
              globals' initial values), triple-XOR resource store, ChangeResourceAmount (stats, XP ->
              level-up, maxLevel 29), AddCrystals, CheckStorageFull, StringToResourceType,
              ExternalResourceTypeToInternal, MetaData + ParseCustomStyleData, full BuildingData
              (LoadBuildingList: upgrades, costs, production, respawn, farm patches, parking/particle
              points, unlock levels), Contracts (deliveries.xml), Splitter.
              Not yet: the rest of GameState::Reset/state, Items (Items::GetItemInfo,
              AddBuildingAsItem are marked UNVERIFIED), the other data files (persons, items, spells,
              locations, sets, shop_packs, collections, tasks, professions, chests, projectiles, tavern).
       3b [ ] Building runtime (Map::Building, 0x1d0 bytes; vtable 0x608100): state from Map::LoadBuidings
              @0x1e2bdc / Patch::SaveBuilding @0x1e1f50, Update @0x1267b0 (construction/upgrade timers,
              contracts, resource gathering, farms), UpdateImage, world click, BuildingHovers.
       3c [x] workers: goblin entities, EntityManager/EntityFactory, pathing and the worker AI.
              Moved here from M4: Building::Update only advances construction while a builder entity is
              at work (BuilderAssigned && BuilderIsWorking), gathering/factories need a working worker
              entity, and farms drive an entity's animation, so the economy cannot run without them.
              Done (2026-10-05, part 1): EntityData/EntityFactory (persons.xml), Animation and
              AnimationController, the AI waypoint graph (CreateRoadAI, weights 1/1000/0.1, links,
              parts) and the Dijkstra path search (TargetedAI::SetTarget), AIBaseState walking
              (UpdateWalking), AIWorker (wandering, GetIdleWorkplace pickup, gathering, building),
              Entity (position, sprite/animation, fades, rings), EntityManager (create/spawn/update,
              in the tick before Map::Update), Building job hooks (AssignWorker, WorkStarted/Ended,
              work/build tiles, parking spots), Map tile queries, blocks, owned borders.
              Part 2: gathering in Building::Update, tree/rock piles (UpdateResources), storage piles
              (UpdateStorage), HireGolbin, Map::GetNearestStorage/UpdateStorageMax, the delivery order
              queue (GameState::PlaceOrder/GetTopOrder) and AIGoblin.
              Verified headless: a spawned worker wanders, picks the idle rock, walks there along the
              roads and mines it; the pile appears, a spawned goblin carries it to the storage and the
              HUD rocks count rises. PORT test aids: `--spawn ID X Y`, `--walk X Y`, `--frames N`,
              `--dump-entities`.
              Building::OnBuilded/OnUpgraded (house workers via SpawnLiver, the farm's farmer, storage
              limit), called from Update when construction/an upgrade finishes.
              BuildingHovers (the city tap layer): hover records for buildings/decorations/entities,
              the hover types (UpdateHovers, SetHoverType), BubbleHoverWindow (build, cut tree,
              finished order, training) and TaxesHoverWindow (taxes with the bonus pulse), dropped
              resources (DropResource, ItemDrop bounce/timeout/collect), the fly-to-HUD moves
              (OnCollect), text popups (ShowTextHover, ShowTextHoverWithStyle + TextStyleManager),
              OnBuildingAssignBuilder, OnBuildingFinishedClick (LaunchContract, AddDeliveryOrder),
              Building gold/CollectResources, GameState order helpers, WindowManager
              DestroyPendingWindows, sprite pixel masks and the world hit tests (buildings,
              decorations, entities). Verified headless: the castle's tax button drops gold piles that
              bounce, a tap flies one to the HUD with "+100 Gold", the rest collect themselves after
              10 s. Not yet: BuildProgress/ResourceRestore hover windows (types 2 and 6), the tapped
              building's info windows (OnBuildingClick, 3e), items/farms/tutorial hooks.
              ResourceRestoreHoverWindow (regrowing trees/rocks) and BuildProgressHoverWindow
              (construction, upgrades, orders), StringTable GetTimeString/GetNumericTimeString/
              GetCountableString, the original's hover refreshes from the worker AI and buildings,
              the goblin's "+n resource" delivery popup. Verified headless: a construction site's
              hammer bubble sends the worker; while it builds the box counts down.
              Done 2026-10-05. Moved out of 3c (they need later systems): decoration jobs and
              AIPlayer (hero world taps, SendGoblinToWork) need MetaExpression, Tasks and the hero
              (milestone 4); farms need saved patch state (3d) and the planting windows (3e): 3f.
       3d [x] saves (done 2026-10-06): SaveManager (blocks/chunks, save file, gzip, temp+prev
              backup), GameState Reset/Save/Load (every chunk; unported systems' data kept as
              loaded), Map::Save/SaveMap/SaveState/SafeSave, Patch::Save, SaveBuilding, Map::Load
              from a save block, SaveEntities/LoadEntities (workers; hero/soldiers kept raw: M4),
              LoadSavedGame, save names, Timer time advance. Saves on exit and app pause; offline
              there is no timed autosave (the original's is for the fb profile only); event saves
              (level-up, placement, land buy) come with their windows. Verified: save -> load ->
              save is byte-identical. Spawns/portals/fog chunks are passed through (M4).
              Moved to 3e: Map::AssignEntities (residents back into houses) with the offline
              building/decoration states and UpdateOfflineResources.
       3e [x] (done 2026-10-06) ShopWindow + BuildingPlacement/BuildingMovement, costs, building
              limits, area buying; economy dialogs (level-up, NotEnoughWindow, ExchangeWindow),
              offline resources, Map::AssignEntities and the offline worker states. The farm
              hovers and the farm-patch halves of the land windows moved to 3f.
       3f [ ] farms: the farm view (Map::ShowFarm, Background::CreateFarm, farm waypoints), the
              patch entities (AIPatch), AIFarmerBig/AIFarmerSmall, Building farm functions
              (SpawnFarm, FarmCollectAndReplant, soil patch states), the farm hovers and windows.
       Note: with no save, LoadSavedGame (@0x1885e0) starts a new game on campaign map 0x15 with the
       hero entity (the tutorial), which needs M4. Until then the port boots the city from a Reset
       GameState (PORT test path).
4. [ ] Hero/army entities and AI (AIPlayer), quests (Tasks, MetaExpression), decoration jobs (Decor
       Update/WorkStarted/teleports), combat, campaign maps (worker entities: 3c).
5. [ ] Sound and effects: music/SFX playback (the GUI and game sound hooks), particles, weather,
       screen effects.
6. [ ] Mac release and polish: .app bundle, settings/persistence paths, Retina/fullscreen,
       performance pass, full playthrough check against the original.

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
  `objdump -d --triple=armv7 --start-address=<ghidra-0x10000> <libkingdom.so>` fills the ranges
  asm_all.txt is missing (it splits some functions, e.g. ApplyViewportLimit; `objdump -T` gives the
  exported global names, e.g. Render::offsetX). The build uses -ffp-contract=off (ARMv7 VMLA rounds twice).
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
- tools/picsym.py: `tools/fn.sh 'X$' | tools/picsym.py` resolves PIC globals (`DAT_x + 0xpc`) to
  exported names / string literals / `[g_addr]`, GOT slot loads (`v = GOTBASE; *(v + DAT_x)`) to
  `&[name]`, and collapses STLport template spellings. Most file-static state (GameState's) is unnamed.
- Dialog pattern (src/windows/): a static FunctionalWindow (ctor gets init/deinit/click/setZ/hide); Init
  sets fn.show/back/update/zRange (+ hideOnOuterClick) and RegisterTopWindow(root). Show: shown = 1,
  MoveWindowOnTop, if the root is hidden WindowShow + tween AnimateIn, root visible. Hide: AnimateOut.
  OnTweenOut (tween onHidden): WindowHide, shown = 0, MoveWindowDown. Static-init order = address order
  of the _INIT_ functions (main.cpp's Queue() list must follow it).
- HUD = HUDWindow::Show: BattleBar, BeltBar, PlayerTop, TopCity, CastleTop, BottomCity, TaskHolder
  (FunctionalWindow statics in a global WindowQueue list; z via MoveWindowOnTop).
