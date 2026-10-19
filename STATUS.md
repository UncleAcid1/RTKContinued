# Port status and handoff

This file is the single source of truth for where the project stands. Update it at every milestone.

## START HERE (new conversation, written 2026-10-18)

**State.** Milestones 1-4 are DONE (closed 2026-10-18, last commit on GitHub main). Coverage:
3437 of 5021 portable functions ported (68%); left: milestone 5 1290, 6 118, 7 176
(`python3 tools/inventory.py --steps`). The game starts a new game, plays the opening tutorial,
the world map and the ambush, and the city tutorial up to step 0x61 ("train your first warrior",
which needs HireTroopsWindow: step 5a). The user play-tested it on 2026-10-18: one bug (the
map-switch sepia was upside down), fixed in e69dc64.

**Next.** Milestone 5, step 5n-3 (LevelInfoWindow opens; 5n-2 DONE 2026-10-19 in 5n-2a / 5n-2b: the windows' profession placeholders, gathering on mission maps, BossIntroWindow's reputation screens). (5n-1 DONE 2026-10-19: Professions + CharacterInfoWindow's tool slot / icons / point items; 5n is split in 5n-1 .. 5n-5, see the roadmap). (5n: Professions, LevelInfoWindow; 5m DONE 2026-10-19 in 5m-1 / 5m-2: SetInfoWindow complete; see the roadmap). (5l DONE 2026-10-19 in 5l-1 .. 5l-3: CharacterInfoWindow complete, with Sets and Collections; see the roadmap). (5k DONE 2026-10-18: 5k-3 the buff row; (5k-2 DONE 2026-10-18: RunRageAnimation and its flight, ArrowAtSlot, GetAdjustedBuffZoneLocation, OnShowManaPotion's belt arrow; (5k-1 DONE 2026-10-18: SpellMovement, the arenas' spell aim, wired in BottomBattleWindow and main; PlaceMagicHoverWindow is dead; 5k is split in 5k-1 / 5k-2; 5j DONE 2026-10-18; 5j-3c done: Update's rage part, Click's drag, OnCenter's run part; 5j-3b done 2026-10-18: OnAutoCast, OnBotAutoCast, HelpWithSpell; 5j-3a done 2026-10-18: the rage bar's buttons, mana bars, OnSpell, OnManaBar; 5j-3 is split in 5j-3a .. 5j-3c, see the roadmap; 5j-2 done 2026-10-18: AISpell, AIWarrior's magic, PlayBuffAnimation, ParticlePresets::OnSpell's delay; 5j-1 done 2026-10-18: Spells + Entity's magic; 5j is split in 5j-1 .. 5j-3, see the roadmap; 5i-3 done 2026-10-18: BossCombatWaves, BossIntroEffect, FastBossIntroEffect; 5i done; 5i-2 done 2026-10-18: BossOutroWindow + the return from a won boss fight, BossTimeHoverWindow, DropBossReward; SetReputation moved to 5n; 5i-1 done 2026-10-18: BaseCombat::SwitchToBossFight + BossCombat, the skeleton boss fight plays; 5i is split in 5i-1 .. 5i-3, see the roadmap; 5h done 2026-10-18: 5h-2 LoadingScreenWindow + SultanArrivingWindow; 5h-1 done 2026-10-18: CampaignCompleteWindow + CampaignPerfectedWindow; 5h is split in 5h-1 / 5h-2, see the roadmap; 5g done 2026-10-18: 5g-2 RaidCompleteWindow + StartRaidingWindow; 5g-1 done 2026-10-18: RaidInfoWindow, the mission brief; 5e done 2026-10-18: 5e-2 RepairWindow; 5e-1 done 2026-10-18: HealHoverWindow, the healing card; 5f done 2026-10-18: 5f-2 ChoosePathWindow, the Navigate button's "where to next"; 5d done 2026-10-18: 5d-2 verified the node -> map 0xe -> step 0x80 and two save round trips; the opening tutorial is complete; 5f-1 done 2026-10-18: CampaignInfoWindow + HintHoverWindow, the world map's campaign node opens its brief; 5f is split in 5f-1 / 5f-2, see the roadmap; 5d-1 done 2026-10-18: the tutorial plays to the world map at step 0x67; 5c done 2026-10-18 in 5c-1 .. 5c-3, see the roadmap; 5c-3 done 2026-10-18: the build / upgrade / task button; 5c-2 done 2026-10-18: the hints, refresh, slot unlock; 5c-1 done 2026-10-18: Train / Hire / the hire glide / Update / OnTweenIn; 5b done 2026-10-18: HireTroopsWindow::UpdateContents complete; 5b-2 done 2026-10-18: timers, locks, refresh / build buttons; 5b-1 done 2026-10-18: the cards' text and buttons; 5a done 2026-10-18: HireTroopsWindow opens, contents empty; 5b is
split in 5b-0 .. 5b-3, all done, see the roadmap near the end of this file:
"5. [ ] The rest of the game").
WORK RULE (user 2026-10-18): ONE step per sitting (about 20-40 min): build, a short check, commit,
push only when the user asks, a 2-3 line plain-language update, then WAIT for the user. Split a
step that grows and say so. Steps marked DECIDED carry the user's answers; ask before anything
freemium/online that is not decided.
Two optional quick preparations the user was offered (not done yet): retry Ghidra on functions that
decompiled to almost nothing (e.g. HireTroopsWindow::UpdateContents 0x2d05e4, 30 KB) with higher
decompiler limits (tools/ghidra_headless.sh, tools/ghidra_scripts); a test-only "save a snapshot
now" option (the game refuses to save during the tutorial, so late-tutorial tests replay it all).

**Standing rules.** 100% faithful port: every function gets `// @0xADDR` (Ghidra address = nm
address + 0x10000); guesses marked UNVERIFIED (milestone N) or PORT; clean code matching the
surroundings; offline-first, online features dormant; no real money (memory/decisions below);
confirm before downloads, installs or anything outward-facing; never share the user's email;
commits end with the Co-Authored-By line; push only when asked.
TEST RUNS: always `--storage <scratch dir>` and `--screenshot <file>`; there is no `--help` (an
unknown flag loads and re-saves the REAL save in ~/Library/Application Support/Rule the Kingdom).

**Tools (all in tools/, see each file's docstring).**
- Reading the original: `out/decomp.c` (Ghidra C, one "// ==== ADDR Name" block per function) and
  `out/asm_all.txt` (asm), both local and regenerable (out/ is git-ignored; README "Regenerate").
  `tools/fnres.py Class::Func` (one function, PIC globals resolved), `tools/asmfn.py 0xADDR` (its
  asm), `tools/dat.py 0xADDR...` (constants), `tools/elfread.py` (u32/cstr at Ghidra addresses),
  `tools/picsym.py | tools/picvar.py` (named globals), `tools/vtable.py`, `nm -D -C` on
  "../Android files/rule-the-kingdom-5-11-multi-android/.../libkingdom.so".
  Ghidra often mixes floats/ints (softfp) and drops values: check doubtful parts in the asm.
- Planning: `tools/inventory.py` (--functions STEP, --unplanned, --steps, --holes) with the rules in
  `tools/port_plan.txt` (first match wins). Both `--functions <step>` and `--unplanned` must be empty
  to close a step.
- Test loop: `python3 tools/test_hook.py on tools/test_hook.txt` (puts a temporary hook in
  src/main.cpp; turn it OFF before every commit), then `tools/testrun.sh SCRATCH BASE NAME FRAMES
  CLICKS` with env SL (sleep frames: jobs/hovers/speed-ups run on wall-clock seconds) and CO
  (collect-a-drop frames). Tap lists that reach each tutorial point: `tools/test_inputs.txt`.
  The hook prints "[t] f=<frame> tut=<step>/<secondTutorial> ..." every 20 frames.
- Run the game for the user: `./build/rtk --root .. --storage ~/rtk-test` (from port/).

**History.** Everything below is the per-step log (newest milestone entries further down; the 4h
block "RESUME HERE" has the details of the last steps). The roadmap with milestone 5's steps is
at the end, before "Open questions".

## Handoff (last updated 2026-10-18: MILESTONE 4 CLOSED. NEXT: milestone 5 step 5a (HireTroopsWindow 1) in the roadmap below; one step per sitting, then wait for the user. The old 4h block ("RESUME HERE") has the city-tutorial test taps. 4g done (campaign combat); 4f done (campaign maps); 4c done (quests core); 4e done (decoration jobs); 4d done (quest HUD and windows)) — read this first in a new conversation
**Where things stand.** Milestones 1–2 done; milestone 3 (city economy) done (3a–3f):
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
   3f.1 DONE: the info windows of a tapped building or decoration (a 3e gap found while scoping the
   farms: factories could not take orders at all). BuildingHovers::OnBuildingClick/OnEntityClick
   (piles; farm patches are 3f)/OnDecorClick and the windows in src/hud/InfoHoverWindows.cpp:
   FactoryHoverWindow (orders: five cells with padlocks, cost/time/reward rows, Order / Change /
   Complete Now / Stop with the PopupSelectionWindow confirm, locked orders -> Improve), the
   BaseHoverWindow frame and Living (houses: Improve, taxes and their countdown bar, residents ->
   camera on them), Storage (space, goblins, amounts white/yellow/red; the hire buttons are only
   shown by the tutorial in 5.11), Castle (Rename, Improve, worker counts), Empty (0x81),
   ResourceHoverWindow (a worked tree/rock: Collect, Speed Up, next unit, worker, Free Up) and
   DecorationHoverWindow (tax decorations). OnResourceAssign (tree/rock tap sends a worker) and the
   "FREE_UP_WORKER" popup (OnFreeWorkerAssign/OnFreeWorkerBuild, also for the builder bubble).
   New: PopupSelectionWindow (two-choice box), Building::Upgrade + UpgradeBuilding /
   UpgradeBuildingContinuation (CallbackUpgradeBuilding*), GUI::DuplicateWindow, the soil-patch
   queries (IsSoilPatch*, GetFirst*SoilPatch, GetNextPatchToBuy), AIBaseState farm virtuals
   (+0xf8..+0x134), Building farmer/patchEntities fields, GameState GetTutorialType /
   GetGoblinCount / GetResourceIconName / GetResourceWorkerIconName, StringTable::GetGenderString.
   Fixed on the way: Building::GetFarmState read the rot timer from patchStart; it is patchArg (+0x9c).
   Not ported (milestone 4): PersonHoverWindow (only campaign maps reach it), decoration jobs and
   quest-locked decorations in OnDecorClick, the quest names in the upgrade lock popup (shows
   "Blocked"), item boosts (Items), tutorial arrows.
   Verified headless (temporary main.cpp hack: finished workshops, a present house and decoration):
   Bakery order with the level requirement, order placed (food charged), Complete Now (4 crystals,
   10 gold reward, order restarts as the original does), Stop + confirm refunds, locked order ->
   "Improve Bakery and Unlock ..." -> upgrade blocked by its quest; house window, Improve -> 6
   crystals -> upgrade scaffold with a builder and countdown; storage window; castle window and
   Rename -> CityRenameWindow; rock tap sends a worker, its window, Speed Up hands over 50 stones for
   5 crystals (the 3e untested item); decoration window. A castle tap first collects its taxes (the
   tax hover takes the tap, as the original); hovers refresh once per real second, so a headless
   run (shorter than a second) cannot tap it twice.
   3f.2 DONE: the farm view and its AI. Map::ShowFarm (farm grid 3x4 of 196x98 tiles at world
   (150, 2880), its own blocks/viewport, Map::CreateFarmPatch), Background::CreateFarm/RemoveFarm/
   GetFarmBounds/SetBrokenFence, the farm waypoints (AI::CreateFarmWaypoints/LinkAdjacentFarm-
   Waypoints/RemoveFarmWaypoints; a pool so waypoints outlive ShowFarm(false)), farm-aware walking
   and SetPos; src/game/AIFarm.cpp: AIPatch (patch states, growth from patchStart, rot with task
   0x429, the powder speed-up and PatchAnimationController, crop entities per order),
   AIFarmerBig (Farm queue, walk, seed/water/mow/showel, WorkTime = 2 plays of the animation);
   Building::SpawnFarm/DespawnFarm/SetFarmPatches/OnSoilPatchBuy/CleanFarm/SpeedupFarm/RestoreFarm,
   the WorkEnded(patch) farm branch (plant, clear rotten, harvest -> DropFarmFood + XP drop + replant
   for the order's gold price, clean), patchProgress (+0x180); BuildingHovers::DropFarmFood and the
   farm-food collect (kind 2), the farm drop bounds (3000/2995, 3100); GameState::
   GetFarmDropImageName; HUDWindow::EnterFarm/ExitFarm; the farm tap (OnBuildingClick class 0xd and
   the farm hover types 8/9/10 in Click) opens the farm view. Verified headless with a temporary
   RTK_FARM hook in main (removed): a built vegetable farm with resourceLeft 2 shows 3 owned patches,
   one for sale, two locked; a growing and a ready crop; the farmer waters the growing patch, idles
   and wanders; Farm(1, 1, false) on the ready patch walks, mows, drops the wheat (it drifts right:
   vx is a constant 30 on the original), cleans, pays 35 gold and replants; ExitFarm returns to the
   city centred on the farm with the farm entities gone; the oil and animal farms render. Known
   gaps: no BottomFarmWindow yet, so in the real game there is no way back to the city or to plant
   (3f.4); EnterFarm shows the farm name on the info line instead of the zooming map-name popup
   (ShowMapName + its HUDWindow::Update animation, UNVERIFIED); sounds; tutorial steps 0x45..0x4c.
   3f.3 DONE: the farm on the city map. Building::SetupSmallFarm (the crop entity +0x15c/+0x160 of
   the first patch with an order, at parking spot 0, z under the farm, farmer just in front),
   FarmCollectAndReplant (the bubble's harvest: storage check, one drop per resource, replant for
   the order's price or leave the patch dirty, AddDeliveryContractToFinishOnLastItem), the crop's
   growth frame in Building::Update (and the first tutorial's farmer hiding), AIFarmerSmall
   (src/game/AIFarm.cpp: seed/water/rest by GetFarmState(0), idle 5-9 s; WalkCompleted's tutorial
   steps, OnTutorialFarmerWomanAction is M4), BubbleHoverWindow's farm parts (ready-crop icon in
   SetBuilding/SetPosition/SetEntity, the farm tap: harvest, else the farm view), UpdateHovers' farm
   branch (ready 9 first, then 8, then 10), SleepingHoverWindow (the "Zzz" for types 10 and 0xb:
   idle farms and also workshops without an order, a 3c gap) with shader type 8 (rotatedVS +
   textPS) in the renderer, Map::FillFarmBuildings/GetFarmAfter/GetFarmBefore, and
   Map::AssignEntities' farm parts (the in-loop farmer case checked against the decompile, plus the
   final pass: a farm without its farmer takes a jobless resident, else a farmer 6/7 living there).
   Verified headless (temporary RTK_FARM3 hook in main, removed): a bought + finished vegetable farm
   with two orders shows the sprouting crop and the farmer watering it (4.3 s, then idles); ready
   crops show full grown with the carrot bubble; tapping it drops 35 food and replants both
   (exactly 5 + 35 gold); an idle farm shows the Zzz with the farmer resting; save -> reload puts
   the farmer back at the parking spot still watering and recreates the crop; a farmer saved
   without a job is given back to its farm by the final pass; tapping the farm still opens the
   farm view. Left for 3f.4: the farm view's ready-patch bubble tap (WORK_RES_NO_SPACE check and
   ShowFarmHover, noted in BubbleHoverWindow::Click).
   3f.4 DONE: the farm view's HUD. BottomFarmWindow (src/hud/BottomFarmWindow.cpp: Return with
   ConvertDroppedFoodToResource, the main button Plant/Gather/Dig/Clean up/Change by the patches'
   states, Improve (with the "orders will be lost" question), Speed Up (the powder for growing
   patches) or Revive (rotten ones, 4 crystals each, confirmed), the sign posts to the previous and
   next farm (Map::GetFarmBefore/After), the tutorial arrows 0x42/0x45/0x4f, HelpWithUpgrade,
   ShowPatchSpeedUp); PatchProgressHoverWindow (the per-patch box during the powder speed-up);
   BuildingHovers::ShowFarmHover (order panel, rotten window, or the progress box following the
   farmer; Entity::SetHoverWindowPositionHandling/GetIdleHeight, SetHoverWindowPosition);
   FarmRestoreWindow (Revive / Weed); OnEntityClick's patch branch (for sale -> LandWindow,
   locked/full-storage notes, else ShowFarmHover); the ripe-patch bubble (UpdateHovers entity
   case, its tap); BuildProgressHoverWindow's farm parts (patch stage text with %.0f, hidden unless
   planting/harvesting/cleaning); LandWindow::SetPatchParameters/OnBuyPatch (prices from the farm's
   farm_patch lists); BuildingPlacement's default patches; HUDWindow SetBottomType 3, EnterFarm/
   ExitFarm, ShowMapName + the map-name zoom in HUDWindow::Update (PORT: only that part runs; the
   original returns early without a player entity, milestone 4); ExitFarm in ShopWindow::Show,
   NotEnough's Find, UpgradeBuildingContinuation; the factory window's per-patch speed-up;
   WindowManager::ProcessBack's farm condition. Not ported (dead in 5.11): FarmGrowHoverWindow
   (created, never shown), OnFarmWater and PatchProgressHoverWindow::SetPartialSpeedUpMode.
   Original quirks kept: the patch window's Revive checks for 4 crystals but takes none (the
   bottom bar's Revive does charge); the soil patch's crystal price is checked and charged as
   listed but shown with AdjustCrystalCost.
   Fixed on the way (gaps from earlier steps): Render::CreateTexture now packs sheets taller than
   2048 px into two columns as the original does (the farm view farmers' mow/shovel animations were
   garbled; any tall one-file animation benefits); EntityManager::SpawnEntityAt(id) registers the
   entity for hovers (the original does; the patch bubbles needed it); BuildingHovers'
   PositionWindow places entity hovers over the head (worldY - idle height - 5).
   Verified headless (temporary RTK_FARM4 hook in main and a test-only task setter, both removed):
   the bar's four buttons and their states; Return back to the city with the farm's lying crop
   dropped again at the farm; tapping the for-sale patch -> "Buy Farm Plot" (1000 gold with a level
   4 lock, 20 crystals), crystals buy -> "Plot Purchased!", the patch is owned and the button reads
   Plant; Speed Up takes 4 crystals and shows the patch box; a growing patch tap and Change open the
   order panel; a ripe patch shows the carrot bubble, its tap or the patch tap sends the farmer with
   "Carrots - Harvesting" following her, Gather queues the harvest; a rotten patch opens Revive/Weed
   (both Revives revive it, Weed clears it to dirty); two farms show the sign posts and the right one
   opens the Oil Farm; Improve shows the quest lock (milestone 4 stand-in); the city side of 3f.3
   unchanged. Headless note: hovers refresh once per real second, and the speed-up box's bar resets
   each wall-clock second, so short runs show it past 100%.
   3f.5 DONE: offline orders. In 5.11 the offline contract hooks are empty (Map::ProcessOfflineContracts,
   GameState::UpdateOfflineBuildings; the offline building list is for the global map, milestone 4):
   workshop orders and farm patches run on global-time stamps, so they catch up on load by
   themselves. Verified headless (temporary RTK_OFF hook and a clock offset, removed): a bakery order
   (300 s) and two planted patches saved, reloaded 400 s later -> the bread bubble and the ripe carrot
   bubble; 100000 s later both patches ripe. Ported the remaining catch-up, the app pause:
   Game::paused/pauseTime (0x6123f9/0x61236c; Android's SDLMain.mcPauseEvent, the port: SDL's
   WILL_ENTER_BACKGROUND/DID_ENTER_FOREGROUND, which desktop systems do not send), main_Loop_Func's
   paused turn (sleep 500 ms, +0.5 s) and Map::OfflineUpdate (@0x1b6754: each building's Update with
   the time paused, once more than 2 s; the decorations' half waits for Decor::Update, milestone 4).
   Verified: a construction site with 29 s left and its builder working finishes after
   OfflineUpdate(100000). Also Timer::ApplyDeltaTimeCompensation (@0x23ea38): the frame time is now
   clamped to 0.2 s as on the original (the port had 0.1 s); TimeCompensationBlock not ported (0).
   4b.1 DONE: src/game/MetaExpression.* (the 154-keyword table of ParseSingle in the original's
   order, generated from the decompile; TP's trailing ",<digits>", the orbs' syntax checks,
   INTERACT_ICON's prefix match; Parse's comment/error skipping and the node chain; FindByType /
   GetDataWithType / FindChildData), MetaData's ParseColon/Wall/SemicolonDividedList and ParseType,
   src/game/Tags.* (GetTag/EnumTag/IsActive; offline every tag is off), src/game/Items.* (Init with
   the quality names "<item_class word> <name>" through StringTable::GetDeclinationString and the
   GIVE_TROOPS "%s" names, InitPacks, InitSpecial "gift_lvl_lock", GetItemInfo by id/name, packs,
   EnumItemInfo, GetRandomItemForSlot, Add{Building,Decoration}AsItem, GetPlayersBestNet,
   GetAmountToBoostTime, all ItemInfo getters), GameState EnumItems and the slot binding tables, and
   the item chunks' info (version 2 drops unknown items; version 3 takes durability). Not ported
   (online): ChangeItemAttribute*, SetTransactionTime. Professions (M5) stubs: IsProfessionTool and
   IsLockedByReputation are false. Verified headless (temporary dump, removed): 682 items + 6 packs,
   407 bonus expressions, no parse errors; "Sharp Antique Sword" (item 193), " Precious Gold Ring"
   (the leading space is the original's: the EN forms are "Precious, Precious, ..."), pack lookups;
   the city and a copy of a real save load as before.
   4b.2 DONE: equipment and item effects. GameState: AddItem (records with durability, first/latest
   acquisition times), RemoveUniqueItem, GetItemAt/BindItemTo and the customisation bindings,
   IsItemBinded, the belt (slot count <= 3, size, item mode, BindBeltItemTo, activation times,
   AddBeltItem/ClearBeltItems), item buffs (ActivateItemBuff, GetItemBuffRemainingTime), recent and
   acquisition times, Setup/UpdatePlayerRegenerationState (HP from the time since mHPTS; mNextHP),
   chunk 0x11's AddItem and the wolf-quest 0x16e fix-up item. Entity: the Add*/Get* stats (absorb,
   crit, fury, initiative, luck, HP regeneration, the +0x84/+0x88/+0x8c combat additions),
   HasRanged/MagicDamage, GetBaseSpeedMultiplier from equipped SPEED, ResetStats' belt reset.
   MetaExpression: OnApplyItemEffect (PARTY: the rest to the squad's soldiers), OnApplyItemBuffEffect
   (the *_PC percentages, +0.5 rounding), CanApplyItemEffect. src/windows/CharacterInfoWindow.cpp: the
   stat part of UpdateContents (ResetStats, equipment, customisations, buffs), run by Map::Load with
   SetupPlayerRegenerationState; LevelUpWindow::Show refills HP; Entity::AddHP updates the
   regeneration state. Stubs: Sets (M5: complete-set effects, AddItem's set quest steps), GIVE_TROOPS
   (M5 hiring: HireSoldier), Tasks::CompleteSubtask calls (4c), OG sharing (online), the belt bar's
   UpdateContents (4g). Original quirk kept: soldiers' initiative is reset only by spawn points, so a
   PARTY initiative bonus adds up with each UpdateContents until the next load. Verified headless
   (temporary hook, removed): sword 193 equipped -> hero initiative 50->65, melee 1->3, soldier
   +5/+1 (PARTY); a cookie HP=1; buffs 685 (HEALTH_PC 30: 9 -> 12/12) and 684 (MELEE_DAMAGE_PC 30:
   3 -> 4); save and reload keeps the item, its binding and the stats; city and real save load.
   4b.3 DONE: items in the world and in the economy. BuildingHovers: DropItem (boost items only with
   "item_boosts", limited items, quest-locked items; no drop image -> straight in),
   ItemDrop::StartAnimation, HasDroppedItem, Collect's item branch (AddItem + BeltBarWindow::AutoBind,
   item 0x26a heals, 0x16c/0x16d count times only, "+%d %s" in the quality's text style), OnCollect's
   item flight (its belt slot, else the inventory button: BeltBarWindow::GetBeltItemLocation /
   GetAdjustedSideButtonLocation). Production orders know their item (Contracts: GetItemInfo(title);
   ~20 of 75 orders), finished orders drop it (OnBuildingFinishedClick), the shop shows the products'
   images, the boost hovers compute GetAmountToBoostTime and use up the item (only with
   "item_boosts", off by default), AIPlayer's map-0xd tap check for dropped item 0x254. GameData's
   load is split into LoadDecorations (before items, as on the original) and LoadBuildings (after
   deliveries); sold/giveable decorations and sold buildings become items 0x8000+/0x4000+.
   NotEnoughWindow: item requirement lines, items in Buy all (price and AddItem), SetItemToProduce
   from the factory hover, the "NEED_ITEM_BUY <item>" button and OnBuyItem (crystals -> the item
   drops at the building). Fixed from 3e.4: Buy all shows only when everything missing can be bought
   (the original's local_c7c: not with a level, people, crystals/XP, buildings, building levels or an
   item without a price; the port had read it as "level reached"). Left: the item Find (needs
   Tasks::HelpWithLocation 4c, NeedItemWindow 4b.4, the global map / item shop / crafting M5),
   profession point items (M5), decoration-job boosts (4e), map 0xb's item arrows (4f), tutorial
   steps around items (4h), sounds (6). Verified headless (temporary hooks, removed): a cookie x2 and
   a sword drop beside the hero and lie there, collecting adds 2 cookies; a NotEnough dialog with a
   level, gold and 3 cookies needed shows the item line and "Buy Donut 5", tapping it takes 5 of 50
   crystals and drops a donut; the shop's Bakery shows its five products; city, reload and a real
   save load as before.
   COVERAGE PASS DONE (before 4b.4): tools/inventory.py is per function now. Every unported game
   function has a step from tools/port_plan.txt (`--steps`, `--functions STEP`, `--unplanned` must
   stay empty, `--holes`, `--unmarked`, `--trace`); tools/funtrace.py traces the unnamed functions
   (callers/address taken, .init_array static initialisers, neighbouring code; none sit in vtables).
   Details and counts: docs/port_inventory.md section 0. It found a new step, 4b.5 (after 4b.4,
   before 4c): holes in finished work. Real gaps: the splash screens, Game::LoadLanguageTable /
   LoadSystemConfiguration / main_Exit_Func (main_Loop_Init/Func are partly in main.cpp without
   markers), ReloadSavedGame / OnResume / MainExitNoSave and the restart/error/language callbacks,
   Background::Update (the horizon parallax), Map::PrepareImages / UpdateNotifications (birds),
   GameState::Update / UpdateNotifications, EntityManager::RemoveEntitiesByID, BuildingHovers::
   MoveWindowDown (the KNOWN BUG's HUD return) / SetArrowAlpha / HideWorldDialog, Map::
   GetTotalStorageLimit / UpdateStorages (stand-ins), GetAreaByID, ShopWindow::FillPatches, GameState
   building counts/limits, the unique-ID checks. The rest of 4b.5 (~200) is "verify and mark":
   helpers ported inline or without their `// @0x` (e.g. GameState::Reset, Render::Init,
   FileManager::LoadFile, RegisterBinaryUI's helpers, FunctionalWindow's virtuals); renderer
   internals get a marker where followed, else a PORT note.
   4b.4 PART 1 DONE: src/hud/ItemHoverWindow.* (the item / resource / text / unit popup: layout
   from Mini_hint and its parts, the 19 action buttons, FinalizeSetup's frame size, SetPosition's
   stacking, RecomputePosition's eight-arrow placement with the fallback to the next side that fits
   (read from the asm), Click with the tutorial holds, SetItem with its type / level-lock / producer /
   effect / troop-stat lines, description, durability bars and the inventory buttons (Drink, Turn
   on/off, Equip/Unequip, Repair, Remove, Improve/Craft, Find, Shop, Place, Open, Ask friends),
   SetEntity's unit stats and Train/Save/Move/Dismiss, the handlers and the Help* arrows;
   GetProbabilityString, ItemPlaceContinuation). MetaExpression's item-effect texts:
   GetItemEffectName / IconName, IsValidItemEffect, GetBonusIcon (its jump table read from the asm:
   Ghidra's case labels are shifted), GenerateDescription (35 KB; Ghidra cut it at its jump table:
   decompiled whole with the new tools/ghidra_scripts/DecompRange.java), GetChanceString.
   src/windows/NeedItemWindow.cpp (all of it: craft / buy, the quest / find / map-object modes,
   OnProduce to the producing building's order or its shop cell, RunLastHelpItem;
   GetBuildingThatProducesItem). UseItemHoverWindow (hover type 0xf; its trigger
   Map::Decor::ShowUseItemHover moved to 4e, it needs the decorations' expressions).
   Items::GetItemOffsetInHolder (both overloads, the per-item id table from the asm),
   Shared::DiscountInfo / ItemIconHolder / ItemHolder. Also: BuildingHovers::
   AllowWindowOverHoverOnce / ShowArrowAtSpeedup / ShowArrowAtUpgrade / ShowArrowAtFactoryContract,
   BuildProgressHoverWindow::HelpWithSpeedUp, IconManager::GetLoadingCogsIcon, Decor::
   GetIconOffsetY (+ DecorData icon_y), EntityData::GetSmallSpellIcon, Entity::HasMeleeDamage,
   GameState::IsArenaCombatActive, PlayerItem::IsBroken, StringTable::StringNotEmpty / GetStringF,
   ToWideString(format, ...) (its own 4-buffer ring), NotEnoughWindow::FindResourceHelp public.
   GUI::RegisterUI's XML-layout branch, root window only (kingdom_icons/*.xml; the child windows
   are a 4b.5 item, UNVERIFIED in GUI.cpp).
   Fixed on the way: NotEnoughWindow's Find arrows now open the building's info window
   (BuildingHovers::OnBuildingClick, ported in 3f.1, had stale stand-ins there) and point at Improve
   (ShowArrowAtUpgrade); ShopWindow::OnBuy deleted the Building it hands to BuildingBought (the
   original's deleting destructor; the port leaked it) and set +0x24/+0x30 as the original.
   Stand-ins (UNVERIFIED, their milestones): Sets / Collections / Professions / Chests / item shop /
   global map / raids / tavern / repair / crafting / CharacterInfoWindow's own holders (the
   equipment bindings are read instead) / the friends hub (online, dormant).
   Verified headless (a temporary --show hook in main, removed): the sword's popup (type, +15
   initiative, +2 damage, squad bonus, durability 40/40, Equip) at the bottom-left arrow, the frame
   sized to its parts; a fury potion (Drink: AP 0 -> 100, the potion used up); orbs, buffs, a troop
   contract; a soldier (20/20 health, Train more, Heal / To Reserve / Dismiss disabled as the rules
   say); a resource and a plain text popup; a cookie (Food, Bakery, Find, Buy disabled): Find
   centres the Bakery, opens its order window on Cookie with the arrow at "Order Cookie"; a hut
   item: Place -> placement -> Confirm builds it at once and uses the item. Holder offsets match
   the XML (belt 611,154; item 0x2c4's -2 correction). City save -> reload and a copy of the real
   save load as before.
   Not yet tested (no owner opens them yet): the popup's hosts (the belt and battle bar slot taps
   are 4g; the character and shop windows 5).
   4b.4 PART 2 DONE (step 4b.4 complete): src/windows/FoundItemsWindow.cpp, the "items found"
   strip that slides up from the bottom over a dark band (Init, Show / Hide with its MovementEffect
   and the band / gold-line sprites, SetZ's half-sunk placement, the ten cells with item-level frames
   and the ContentScroller strip with its arrows, OnItem's own ItemHoverWindow, Ok / "tap to
   continue" (HideOk, the blinking glow text), Go to shop / tavern (arena only), OnBack,
   AddCollectedItem / AddCollectedResource with their priorities, ClearCollected,
   GiveCollectedToPlayer (drop in stacks or add; crystals capped at 15), SetParameters from a setting
   ("GOLD|200;133|4", unused in 5.11) and from a map's collected loot, RunPostAction).
   Also: GameState::Add/Get/ResetMap*CollectionInfo (the saved per-map loot record, key map << 16 |
   item, or | 0x2000 + resource), Render::CreateSolidFillTexture.
   Its owners call it then MoveWindowOnTop themselves, which is what gives it depths (its own
   MoveWindowOnTop runs before it counts as shown). The first show slides to the screen's bottom
   edge and the next re-stack lifts it to the band's middle (g_offsetY is computed in SetZ), as on
   the original.
   Deferred with their structs (tools/port_plan.txt rules by address, a new `@addr` rule form):
   SetParameters(Tasks::TaskInfo*) 4c, SetParameters(Locations::LocationInfo*) 4f.
   Left out: the PvP arena's path (PlayerBattleInOutWindow 4g); the share ticks and wall posts
   (online). RunPostAction's / OnClose's windows are comments until their milestones.
   Verified headless (temporary --show hook, removed): six cells (resources and items, amounts),
   fourteen items scroll with the arrows and by drag, a cell's popup ("White Knight", pointing at
   it), Ok closes, "Tap to continue" glows and any tap closes, map 7's loot from the collection
   record, daily_bonus_chest_1 / new_daily_bonus_1 from settings, "No items found", giving: gold
   0 -> 600, crystals 20 -> 15. City save -> reload and a copy of the real save load as before.
   4b.5 PART 1 DONE (verify and mark, no behaviour change except one fix): every `--unmarked`
   name checked against the original. Marked where the port already follows it (WindowManager's
   FunctionalWindow virtuals and WindowQueue::Click/Update, Map getters, GameState::Reset, the
   SetAnimation std::string overloads, EntityFactory::Init (a branch to InitNames), FileManager::
   LoadFile's size-less overload, SaveChunk's Save* wrappers, GetData<T> / BeginChunkLoading, the
   SystemFuncs settings, AnimationController / Building getters, Timer::Init, GetRealGlobalTime,
   GUI::GetFont, SetOnClick's overloads). Re-placed (port_plan.txt "placed by the 4b.5 review"):
   overloads and stubs of later steps (barracks training 5, OnApplyItemEffect(int*, bool) 5,
   HUD OnFarmTimer 5 / OnFindTask 4d, QuickTaskWindow 4d, BattleBarWindow::Update 4g, spawn-point
   SpawnEntityAt / GUIDs / IsWindowHigherThan 4f), dead WindowManager helpers, container helpers
   (util). Fixed: PlayerTopWindow::Init now wires the portrait's clickArea (no-op OnCharacter, no
   sound), as the original does. Found holes for later pieces: StringTable::GetUndecoratedString
   (the text popups' decoration pass), Map::Free's missing parts, AI::SetWeight in main_Loop_Init.
   4b.5 PART 2 DONE (comments only): the renderer and Resources internals. Render.cpp records the
   original's context/atlas/layer/shader/sprite-pool/texture-pool functions the SDL3/OpenGL renderer
   replaces (with what stands in); Init, SetWrapping, RemoveTexture, GetDirectImage marked;
   Resources.h records the start-up pack registry (RegisterResources/RegisterAnimations and their
   tables) the port's on-demand probing replaces. UNVERIFIED notes: the colour-mask shader path
   (ApplyMask, callers in 4g/5), Textfield::SetRotation's minify filter, exploded animation frames.
   4b.5 PART 3 DONE (review of the rest; small fixes): every remaining 4b.5 function sorted by its
   callers (asm calls plus vtable/GOT entries in the binary). 107 functions are `dead` (no caller in
   5.11), others moved to their steps (tutorial 4h, decoration jobs 4e, quests 4c, towers/PvP 5,
   friends online); helpers marked (GameState's worker forwards, name lookups, DecorData::GetImage,
   entity/building getters, CopyEntityData, CreateShadowGlowFilterSet: no entity uses it in 5.11).
   Fixed: FunctionalWindow::UnRegisterTopWindow ported (part 1 wrongly called it dead: it is
   virtual); the coverage check ignores range starts `@0x..` and unported references are now written
   `(0xADDR)` (AddSmoke, road painting, CreateGradient ... no longer count as ported). The 66 left
   are listed in docs/port_inventory.md section 0 (part 4: in play; part 5: start-up/shutdown).
   4b.5 PART 4a DONE (70e5b3b): Map::GetTotalStorageLimit @0x1b7224 (the top bar's storage limit;
   away from the city the last non-zero one) and Map::UpdateStorages @0x1b8b74 (every storage's
   piles follow resource changes), replacing their 3c stand-ins.
   4b.5 PART 4b DONE (28f7d2c): the city's ambient birds and clouds (Map::PrepareImages @0x1bb4d4:
   images/bird/, three looping chains of four), Map::Update's timers and Load's tail,
   Background::Update @0x10b238 (the mission maps' horizon parallax), Map::UpdateResources @0x1ba5ac,
   the Allow*VisibilityUpdate flags, GameState's time modifier (Set ignores values above 1) and the
   map start/visit times. Re-placed: GameState/Map::UpdateNotifications (the phone's local
   notifications) to 5, EntityManager::RemoveEntitiesByID (the queued map switch) to 4f.
   UNVERIFIED (their milestones): the queued map switch, fog, Decor::Update (4e), the tutorial task
   fixes, the map-version update, the "map_timer_ding" sound.
   4b.5 PART 4c DONE (540847a): patches made from the area list (Map::AddPatches @0x1bee2c for the
   city, AddOnePatch @0x1c0b90 for other maps, GetAreaByID @0x1bc954), AssignUniqueIDs @0x1b7c0c,
   CreateRandomDecors @0x1bb7c8; ProcessOfflineContracts @0x1b7434 marked (empty). PORT: a missing
   patch is skipped (the original goes on with a null patch and crashes). Dead: ShopWindow::
   FillPatches (no shop tab of type 5 in 5.11) and Map::GetUniqueIDCount.
   43 LEFT IN 4b.5 (`python3 tools/inventory.py --functions 4b.5`), listed in docs/port_inventory.md
   section 0: 13 in play (part 4d) and 30 start-up/shutdown (part 5).
   4b.5 PART 4d DONE (the rest in play; 13 functions): Building::AddSmoke @0x11e898 (the smoke entity
   0x17c "smoke_1" over workshops 0x6b 0x3ef 0x86 0x66 0x65 0x6a 0x3f0 at the particle point of
   their level, from LaunchContract, UndoAction, OnMoved and UpdateOfflineStateNoWorker) with
   Entity::GetAnimationFrameCount @0x151168; OnContractCompleted now removes it (+0x170; the port
   only dropped the pointer). Map::Free @0x1bf768 completed (decorations then buildings per patch,
   the farm patch, waypoints, clouds, every entity's OnMapUnload @0x150764, SoldierPool::
   DeactivateDeadSoldiers, EntityManager::Clean(false), SetBottomType(0), the interaction locks,
   birds; UNVERIFIED with their milestones: particles 6, spawn points/portals/fog/queued placements
   4f, TutorialWindow::ResetState 4h). GameState::Update @0x1964ec (only online paths set
   saveOnline; the Facebook reminder is online) and UpdateTagState @0x185964 (once per second,
   Tag +0x05 latch; EntityManager::OnTagUpdate 4c and HUDWindow::UpdateTasks 4d are UNVERIFIED),
   both in the tick. StringTable::GetUndecoratedString @0x22c1e8 with FUN_0022c08c: "%s ... %s[player]
   [castle]" texts (player, castle, item:<id>, building:<id>; the task fallback is 4c), used by
   ShowTextHover and ShowTextHoverWithStyle. BuildingHovers::SetArrowAlpha @0x264b94 (PopupWindow
   Show 0 / OnTweenOut 1) and MoveWindowDown @0x2642dc. Re-placed: GameState::GetBuildingCount
   dead (its only caller is BuildingMovement's warehouse mode 4, never switched on; the counts are
   never written), BuildingHovers::Show/HideWorldDialog to 4f with WorldHintHoverWindow (the
   talking-NPC dialog; only the tutorial, campaign, combat and PvP open it).
   KNOWN BUG FIXED (Settings gear dead after a building window): HUDWindow::Update's HUD return is
   ported (the port's Update also no longer returns early when the map-name popup is hidden). The
   missing piece was in main_Loop_Func (@0x188e68): each frame, before the events, desktop_window
   is moved back to the bottom of the window queue when it is not the tail. So MoveWindowDown puts
   BuildingHovers just above it and building taps keep working (in main.cpp at the end of tick).
   Verified headless: bakery window -> close -> the gear's press and release reach CastleTop and
   Settings opens (the unchanged build sent the release to BuildingHovers); Settings' X -> the
   bakery opens again; the shop, Tools move + Cancel and a building tap after it work; the smoke
   rises from the bakery's chimney (temporary hook, removed); "[player]/[castle]/[item:363]/
   [building:107]" texts resolve (temporary hook, removed); fresh city save -> reload keeps the
   hero and soldier; a copy of the real save loads as before. Found, not caused by this step: on
   the PORT test path (no save) Map::Load prints "SetupPlayerRegenerationState() ... HP has
   decreased from 10 to 9" (the unchanged build does too; the hero is made before the HP timer).
   4b.5 PART 5 DONE (step 4b.5 complete; 30 functions): start-up and shutdown. src/main.cpp is now
   the original's functions: Game::main_Loop_Init @0x18d850 (every call of the original in its
   order; the unported ones named in place with their milestone), main_Loop_BeforeInit,
   main_Loop_Func @0x188a40 (one frame), main_Exit_Func @0x185200, RepeatedMain, and main as
   SDL_main. In src/game/Game.cpp: Game::LoadLanguageTable / LoadSystemConfiguration (a file from
   res_files/1Original, then each active content pack's copy: online, dormant) / LoadEntityFactoryData,
   OnResume (feature holders 4d and the daily bonus 5 are UNVERIFIED notes), turnMusic (sound: 6),
   MainExitNoSave, FileManager::RestartGame / SetPreferredLanguade, LanguageSelect / LanguageDecline
   and the question itself (AskLanguageQuestion, a part of main_Loop_Func).
   Findings: Game::splashLoop, OnShowRestartGame and GameState::Clean are empty in 5.11 (there are no
   splash screens; the loading screen is Render::UpdateProgress, milestone 7).
   Languages: English is always loaded, then the language's own table on top (RU DE LV ES IT FR KO
   JA; all eight load). A new player (no save) gets the system's language; a player with a save and
   no choice yet is asked once ("Languages Available": Okay saves, stores the choice and restarts
   the game; Cancel keeps the current one). `--lang=xx_XX` and `--empty_strings` are the
   original's own switches (SDL_baseInit). PORT: Android's Java side supplied the system language
   and the stored choice (not decompiled: UNVERIFIED); the port reads SDL's preferred locale and the
   "selected_language" setting. PORT: Korean and Japanese are not offered automatically, because
   their font is Android's /system/fonts/DroidSansFallback.ttf (boxes without it; milestone 7).
   PORT: a restart (FileManager::RestartGame) starts the program again instead of looping in-process.
   PORT: SystemFuncs settings are now kept in <storage>/settings.txt (read by SystemFuncs::Init,
   written on every change), so the language choice and the settings window's mute switches
   survive a restart (Android: SharedPreferences).
   Also ported: GameState::InitDefaultSettings @0x1aa0e4 with the server switches (Is/Get/
   SetServerSetting, the "TSS_..." names; TSS_MAX_LEVEL 29 sets maxLevel, TSS_DELAY 0 is the frame's
   SDL_Delay), ResetGamePauseState; Contracts::InitSpecial @0x12eed8 (the training order lists
   0x800..0x802 of buildings 0x8e, 0x69, 0x93, three units each, from EntityFactory::
   GetEntitiesForBuilding and EntityData::GetCostGold / GetCostCrystals; their windows are
   milestone 5); Map::BuildingDeinit / DecorDeinit (GameData); AI::SetWeight (1.41: no required
   content packs); StringTable::SetEmptyNameReplacement; Render::GetScreenViewWidth/Height;
   WindowQueue::ProcessWheel; EntityManager::Init.
   main_Loop_Func changes from reading it whole: the wheel does nothing on a farm and goes to the
   windows when one is shown (ProcessWheel); a press held over 0.2 s is a long tap (GUI::
   OnLongTapStart / OnLongTapAbort); the background save also closes the Tools mode; the frame is
   skipped once an exit without save is set; coming back to the foreground runs
   SetupPlayerRegenerationState and OnResume. main_Exit_Func: the save, Map::Free,
   EntityManager::Clean(true), the desktop window, the data lists; the other systems' Deinit are
   left to the program's end (port_plan "implicit").
   Verified headless: fresh city, save -> reload -> save byte-identical from the second reload,
   through the exit path (`--quit`); a copy of the real save loads and saves on exit; with a save
   and `--system-lang 2` the question appears, Cancel stores language 0 and it is not asked again,
   Okay stores 2, saves and ends, and the next start is in German; every language's table loads;
   Settings, the shop, the back key's exit question and walking work as before.
   Not testable headless (please try in the real game): the window's close button and Esc now go
   through main_Exit_Func; the restart after choosing a language.
   Headless test aids added: `--quit` (run main_Exit_Func after the shot: saves into --storage),
   `--system-lang N` (the system's language id 0..8). A headless frame is now AfterEvents + Tick
   (edge scroll, long tap, the language question, as in the game).
   4c PART 1 DONE (51 of 150 functions): the quest data and its saved state. src/game/Tasks.*:
   Tasks::Init @0x23aa70 (tasks.xml: 1371 tasks, 1021 steps; every <XCondition> flag, the 67 step
   types by name, "androidVal" over "val", amount ranges, button text / icon, the reward's
   resources / gold / "cb" crystals / XP, reward and give items, name / description / pre-quest
   strings, post_quest -> previousTask, the repeat lists), TaskInfo / TaskSubtask (layouts in the
   header), the lookups (GetTask, GetSubtask, GetTaskWithSubtask, EnumTask,
   GetCampaignCompletionTask), the map queries (GetMapTasks / Count / Completed /
   LatestUnfinishedTask, CompleteMapTasks, ResetMap), progress (GetTaskProgress / SubProgress),
   IsTaskActive, TaskInfo's GetIcon / HasTimeout / GetRemainingTime / CanBeCompleted / LockByTask(s)
   / IsLockedByTask / GetFirstLockedTaskInChain / IsFeaturedTask / RefreshCampaignInfo (Locations:
   4f, UNVERIFIED), Reset, SetBlocked, EnqueueUpdate, the last-step-help setters. GameState: Set/Get
   task show / begin / end times, the step counters (GetSubTaskDone creates the counter, as the
   original), AddSubTask, SubTaskCompleted, the custom step requirement, CompleteTask, StartTask
   (EntityManager::OnTaskStarted: part 3), ResetTask, AddAutocomplete/AutoresetTask. tasks.xml is
   loaded at start-up in its original place; GameState::Reset calls Tasks::Reset.
   FOUND AND FIXED (asset layering): FileManager::RegisterBlobFile @0x16e7b8 leaves out four files
   of main pack version 27 (the user's files/main.27.kbf): tasks.xml, LocalizedStrings.xml,
   config/fbwall_mob.xml and maps/map_11.bin (and version 28's two loading images). The port
   loaded all four from the old pack: an older tasks.xml (998 tasks, no task 0x771), the older
   Russian table and an older map 11. Now 5.11's own are used, as on the original.
   Verified headless (temporary dump, removed): 1371 tasks, the lists, sample tasks with their
   steps, locks and chains; city, real save copy and Russian start as before.
   Note on "byte-identical" saves: three time stamps (map start time chunk 0x1d, chunk 0x1f, map
   visit time chunk 0x55) are set at each load, so two saves only match when the runs fall in the
   same second; everything else is identical.
   LEFT IN 4c (99; `python3 tools/inventory.py --functions 4c`): part 2 the quest engine
   (Tasks::CompleteSubtask / CompleteTask with the rewards / Update / GetCurrentTasks /
   CheckOverlimit / LaunchDailyTaskFromGroup / TaskTimeout, descriptions, SetAttribute), part 3
   the world's reactions (Entity / Decor / Map OnTaskStarted / OnSubTaskCompleted /
   OnTaskCompleted / OnSetup / OnInteract / OnTagUpdate, ExecuteStaticMeta), part 4 help and
   extras (TaskSubtask::OnHelp, HelpWith*, Bonus, Override, the gem messages, the admin console
   commands). ASKED THE USER (freemium rule) before part 2: the crystal rewards of quests; only 4
   tasks give crystals in 5.11 (224: 1, 245: 5, 1183: 2, 1234: 1).
   BALANCE PLAN (2026-10-11): docs/balance_plan.md, CONFIRMED by the user: crystal prices keep
   their first 20 and lose 55% of the rest; units cost 40% of their crystals AND food (original crystals x 40
   food, at most 20,000); crystal income from quests / level-ups / dailies /
   chests / arena scaled by a 1x-3x level factor; the gold-for-crystals exchange limited to
   10 + level a day. Every number goes in a setting. Follow it when coding rewards and prices.
   4c PART 2a DONE: the quest engine. Tasks::GetCurrentTasks @0x230dec (which tasks are open:
   level, locks, the place rules by GameState::GetCurrentLocation, hints / tutorial pop-ups after
   their previous task, the HUD's filters, priorities; the campaign parts wait for Locations, 4f),
   CompleteSubtask @0x2318a0 / ResetSubtask / CompleteSubtaskDirect, CompleteTask @0x236350 (the
   reward dropped at the hero - or, 4f, at the quest giver - else added; reward items; items taken;
   the original pays a task's "cb" crystals only up to 5), Update @0x2377b0 (once a second in the
   city, in the tick after BuildingHovers: begin times, the balance / item / level steps, finished
   tasks, disable_task, repeating tasks coming back, the daily launch every 24 h),
   LaunchDailyTaskFromGroup (daily_tasks + daily_herb + daily_min, and daily_pvp), TaskTimeout,
   WindowManager::GetEnqueuedWindowCount, GameState event start time / difficulty and
   IsFeaturedTaskClicked. Actions already in the game now report their steps: building built /
   upgraded / placed, order collected or sped up, level reached, hero healed, first training.
   PORT (balance plan): a completed quest also pays PortQuestCrystals (settings
   port_quest_crystals_main / _side / _campaign / _daily, port_level_factor_div); a level-up pays
   level_up_cb + level / port_levelup_crystals_div. tools/balance_sim.py prints the plan's
   prices and income from the data files (its first run: about 760 one-time crystals over the
   campaign, 17 a day afterwards before chests / hunts / arena: below the plan's estimate, to tune).
   PORT until 4d: a finished task with a reward is completed at once (the original opens
   TaskCompleteWindow, which completes it when closed); chapter tasks wait for ChapterWindow.
   Verified headless (temporary hook, removed): task 232 (build house 1006) opened by completing
   its lock tasks, the step reported, Update completed it and its gold, XP and one crystal dropped
   at the hero; the fresh city and the real save copy complete only the step-less task 1182; shop
   and settings work as before.
   4c PART 2b DONE: Tasks::CheckOverlimit @0x236f28 (each Update: open steps whose goal is already
   met are completed, then the task - an area owned, no area left to buy, enough storage goblins,
   an item of type 1 / 8 owned, what the hero wears reported as equipped; with withMobs the "kill"
   and "get item" steps whose listed monsters are all dead, which waits for spawn points, 4f),
   Map::GetNextAreaID @0x1b7264 (Patch::price = AreaInfo "c"), TaskSubtask / TaskInfo::SetAttribute
   and ChangeSubTaskAttribute / ChangeTaskAttribute (Override::Apply's setters, part 4; they name
   two fields: +0xf8 "active", +0x10a "no_complete"). TaskSubtask::GetDescription @0x231ac4 is
   DEAD: no caller, no address taken, and its texts (TASK_BUILDHOUSE ...) are not in the 5.11
   string tables (decompiled whole with DecompRange to be sure; port_plan.txt). More actions report
   their steps: order started (0xc), drops collected (0xd), item gained (0x20), area bought (6 and
   0x3b), farm patch bought (0x2b), hero healed by regeneration (0x27), the wolf-quest fix (step
   0x323); leaving the farm runs Tasks::Update(true). Building::OnUpgraded reports Building::id as
   the original (+0x18), not data->id.
   UNVERIFIED left in CheckOverlimit, each with its step: kInteractBuildingAny needs
   Map::GetDecorationToClick (4e), kMasteryLevel Locations::GetMissionStarCount (4f), kImproveItem
   and the "or better" / equip chains Collections (milestone 5).
   The original's other CompleteSubtask callers are in code not ported yet and come with it:
   Map::Update / Map::Load enter-map (0x1e; 4f / 4h), BuildingMovement::Accept decorations (2; 4e),
   Decor::Update and AIPlayer::ProcessAction (0x22 / 0x23; 4e), Entity::OnDeath / OnHit / OnTalkTo
   and Map::UpdateSpawns (4f / 4g), hiring, crafting, repair, item shop, arena, raids (milestone 5).
   The whole list: awk over out/decomp.c for "Tasks::CompleteSubtask(".
   Task texts in windows still waiting (all task UI, 4d): the shop's and the hire hover's "needs
   quest" texts (ShopWindow.cpp, InfoHoverWindows.cpp), LevelUpWindow's "new tasks" cell, the
   exchange's discount tasks, NeedItemWindow's task mode, GetUndecoratedString's task counts.
   Verified headless (temporary hook, removed): task 1281 (own area 3 + get 3 of item 121): after
   Map::BuyArea(3) without a report the next Update counted the area step; the three AddItem calls
   counted the item step; the task completed and opened 236 and 1505. An item of type 9 owned
   before its task opened does not complete a "get item" step (as the original: types 1 and 8
   only). Fresh city and the real save copy: as before (only the step-less task 1182 completes).
   4c PART 3 DONE: the world's quest reactions. A decoration's MetaExpression is now parsed when
   the map loads (Decor::meta +0x20, Decor::SetMeta; metaText stays for the save; a building
   record replaced by a decoration hands its expression over, as the original) and Decor has
   +0x41 `disabled`. Decor::OnSetup @0x139190 / OnTaskCompleted @0x138ae4 / OnSubTaskCompleted
   @0x138430 / OnInteract @0x1341d4 (HIDE_ / UNHIDE_ / UNHIDE_AFTER_ / REPLACE_ / DISABLE_AFTER_
   for QUEST, SUB_QUEST and INTERACT, REPLACE_OBJ, RANDOM, TRAP_DAMAGE), SetBlockedTiles /
   RemoveBlockedTiles (the BT list), Get{Start,End}QuestID, Map::GetDecorationToInteract /
   GetNearestFilteredDecoration, Map::ExecuteStaticMeta / OnTaskCompleted / OnSubTaskCompleted /
   OnInteract, Map::EnqueueBuildingPlacement @0x1b8724 with its half of Map::Update (one queued,
   finished building is placed each update: in the city a REPLACE_* naming a building turns the
   ruin into that building). src/game/EntityMeta.cpp: Entity::OnSetup @0x15a938 / OnTaskStarted /
   OnTaskCompleted / OnSubTaskCompleted / OnInteract / OnTagUpdate (DIRECTION, ANIM_IDLE, PATROL,
   HIDE / UNHIDE parts, ANIM_AFTER_*, ANIM_FINISH_QUEST, GOTO_AFTER_* with hide / unhide / dialog,
   RANDOM, STATS_REPLACE_AFTER_QUEST, ARENA_SPAWN, MOB_SPAWN, UNHIDE_TAG), GetMetaExpression,
   SetForeignMetaExpression, IsEnemy, Get/SetGUID, AddPatrolPoint @0x15854c (Entity::patrol; nothing
   walks the points until 4f); EntityManager::On* and ExecuteStaticMeta. src/game/SpawnPoint.h:
   only the fields these use (4f loads spawn points, so until then no entity has an expression).
   Wired: Tasks::CompleteSubtask / CompleteSubtaskDirect / CheckOverlimit / CompleteTask,
   GameState::StartTask, the tag update in Game.cpp, Map::Load (the player's OnSetup,
   EntityManager::ExecuteStaticMeta, Map::ExecuteStaticMeta with AI::allowWaypointLink off).
   The hidden arguments (softfp-free but dropped by Ghidra) were read from the asm: switching on
   is SetActive(true, false), off SetActive(false, true), computed SetActive(x, true); fading in
   Appear(true, true), in OnTagUpdate Appear(false, true).
   UNVERIFIED inside these, each marked with its step: particles (Decor::CreateParticles /
   RemoveParticles, 4e), camera paths and shakes (AddCameraCinematic / AddCameraPoint /
   AddCameraShake, 4f), spell effects (ParticlePresets::OnSpell, 6), CRAFT nodes (Professions, 5),
   ARENA_SPAWN's entity switch (GetEntityForArena, 5), PVP_SPAWN (Entity::ExecutePvPSpawnMeta, 5),
   RestoreDecorationInfo (4e), the tutorial-0x3a callback of a placed building (4h).
   Moved in port_plan.txt: ExecutePvPSpawnMeta, MetaExpression::OnApplyItemSquadEffect and
   GetSummaryEffectsValue to 5 (crafting / set windows, GameState::GetBIS); dead:
   Map::GetEndQuestTileLocation.
   Verified headless (temporary hook, removed): the city has 9 decorations with an expression
   (ruins: REPLACE_AFTER_QUEST=<building>:<task>, REPLACE_OBJ, REQ_UNLOCK_QUEST ...); completing
   task 1281 turned decoration 1493 at 14,17 into the finished building 101 and the ruin was
   gone; fresh city and the real save copy load, save and reload as before.
   4c PART 4 DONE - 4c IS CLOSED (`inventory.py --functions 4c` prints nothing). Ported:
   src/game/Override.* (AddParameter @0x1da7ac, Apply @0x1daad4: a queue of named data changes;
   the game queues one itself at start-up - building 99, the castle, upgrade 1 "r_cb_limit" = 75 -
   and main_Loop_Func applies everything each frame; the kinds only the server sends are listed in
   Apply as dormant; the original's fall-through that drops the entry after a skipped location
   parameter is kept), GameData::ChangeBuildingAttribute (Map:: @0x128e38; BuildingData gained
   +0xb4 exchangeLimit and +0x31 giveable), Map::ChangeObjectAttribute @0x1bca94 (the table only:
   Map::LoadPlayer's use of it is online), Map::GetBuildingWithContract / GetUpgradeableBuilding /
   GetFarmWithPatchToBuy, GameData::GetBuildingDataWithContract, GameState::TryToHelpWithTask
   @0x193978 (the "Complete quest '<name>'" pop-up) and AddEntity (empty in 5.11),
   FoundItemsWindow::SetParameters(TaskInfo*, title, crystals) @0x2bcffc (order table 0x597300:
   crystals, XP, gold, resources 0..7, then the items).
   RE-FILED in port_plan.txt, with the reasons:
   - 4d: Tasks::TaskSubtask::OnHelp (1244 lines: the task window's "Go" button, a switch over the
     67 step types into shop / item shop / hire / craft / world map / character windows and the
     arrows), HelpWithLocation, HelpWithMapSwitchTo(+Continuation), RunLastSubTaskHelp,
     ShowWarriorSlotUnlock. Every caller is quest UI (TaskInfoWindow::OnHelpActual, TaskListWindow)
     or later, most of its targets are 4d / 5 windows, and it can only be tried by pressing the
     button: it is the first thing to port in 4d. Its jump table must be checked against the asm.
   - online (dormant): Bonus::* (gifts from the server / Java / console), the gem step message
     (ServerFlags' blocking ToS / exit message in Update).
   - removed: Tasks::SendTasksFootprint (sent with a real-money purchase).
   - 5: the console commands for tasks and tags (admin mode), GlobalMapWindow::OnTaskCompleted,
     BuildingHovers::CheckForSpecialDrops.
   DECIDED by the user (2026-10-11): the Ruby / Diamond Pyramid events stay (tasks 0x4d7..0x4db;
   BuildingHovers::CheckForSpecialDrops drops the pyramid item with the "ruby_find_chance" setting
   while its task runs). In 5.11 finding one gave 30% / 40% more gold and crystals on real-money
   packs for 24 h (ExchangeWindow::UpdateCosts' multiplier); offline the same bonus applies to the
   port's gold <-> crystal exchange for 24 h. To do with the exchange in milestone 5: port
   CheckForSpecialDrops, the multiplier, and decide what starts the events offline (nothing does
   yet; 5.11 started them from the server).
   Verified headless (temporary hook, removed): building 99 upgrade 1 has exchangeLimit 75 after
   the first frame; queued task / step / building overrides apply by kind and with kAll;
   TryToHelpWithTask(1281) shows "Complete quest 'Stonemason's Conditions'"; fresh city, real save
   copy and the building shop as before.
   4d PLAN (user confirmed 2026-10-11): part 1 the quest icons on the HUD (TaskHolderWindow,
   Shared::TaskWindow, HUDWindow::UpdateTasks which is a jump to TaskHolderWindow::UpdateTasks);
   part 2 TaskInfoWindow and the "Go" button (TaskSubtask::OnHelp and its helpers); part 3
   TaskCompleteWindow (replaces Tasks::Update's "completed at once" PORT); part 4 TaskListWindow,
   LevelInfoWindow, HelpWindow, the locators and remaining arrows; part 5
   TaskQuickCompleteWindow (pay crystals to finish): ASK THE USER FIRST.
   4d PART 1 DONE: the quest list of the HUD. Shared::TaskWindow (Shared.h: LoadFrom @0x342ef0,
   Animate @0x3408dc; the field table is in the struct) and src/hud/TaskHolderWindow.cpp: Init
   @0x355384 (32 buttons "Slider_mission_scroll_bg", 32 + 32 banners "New_task_huge_container" /
   "New_task_container", the scroller with "Scroll_bar_tiny_container", QuickTaskWindow
   "Quick_task_line_holder"), UpdateTasks @0x356048 (= HUDWindow::UpdateTasks @0x2c7ba8),
   UpdateContents @0x358c70, Update @0x359680, SetZ, Click, Hide, ResetTasks, RemoveTaskArrows
   (+ HUDWindow's), SetTaskCharacter, TaskOrdering, OnTaskInfo @0x3591e8, Show / Hide
   QuickTaskDescription, SetTaskVisibility. Every "HUDWindow::UpdateTasks" marker in Tasks.cpp,
   Game.cpp, HUDWindow.cpp, LevelUpWindow.cpp, BuildingHovers.cpp and HoverWindows.cpp is now a
   call. Read from the asm (Ghidra lost them): the background logic of a button (registers: r6
   campaign button, [sp+0xf4] campaign done, r8 raid, r7 "special", r10 desert / arena kind), the
   texts ("%d/%d" a quick task's counted actions; "%d%%" campaign percent; PROGRESS_ARROW with
   two ToWideString numbers), the timers (a "new" banner 8 s, 32 s in the first virtual tutorial
   and second-tutorial steps 0x208 / 0x20c; a progress banner 8 s; the campaign button rotates
   every 8 s), Init's numbers (x -5, top 65 * hud scale, bottom margin 170 (+35 above 600 px),
   gap -25, banner offsets -60/-5 and -30/-5, scale 0.96 * hud scale on high DPI else 1).
   UNVERIFIED inside, each marked: everything of Locations (campaign done / percent / mission
   time: 4f), the arena map's task 0x5da and OnTaskInfo's other windows (5), expansion task 0x5eb
   (online), PlayerTopWindow::IsPlayerDialogVisible (4f), the "ui_new_quest" sound (6),
   BattleBarWindow::ShowAllTasks / HideAllTasks and ExecuteFirstHelp @0x359414 (4d part 4; the
   latter is the autoplay bot's "tap the first quest"), TaskInfoWindow (part 2: OnTaskInfo only
   prints "Showing task N"), who calls ResetTasks (Map::Load, GameReload, GenerateNew: with them).
   PORT: UpdateTasks returns when the HUD does not exist yet (LoadSavedGame asks before Init).
   Verified headless (temporary hook, removed): a city jumped past the tutorial has no visible
   quest (all 21 open ones are dn_show or "talk to" quests: correct); after completing tasks
   1279 / 1012 / 230 / 992 three buttons slide in with their pictures and "New Quest!" banners,
   the banners leave after 8 s, a counted step shows "Progress 1/3", a tap prints the task; the
   real save copy and the building shop as before.
   4d PART 2a DONE: src/windows/TaskInfoWindow.cpp, the quest window (Task_info_win.xml): Init
   @0x35d514, SetTask @0x35afb4 (title, giver by the hero's sex, text with a smaller font except
   in English; three step boxes: icon with mask, "%d/%d" count ("%d%%" for a campaign step),
   rings, the map's name when the step is elsewhere, the action button with the step's own text
   and icon, greyed until its required steps are done, or the "skip" box "%s %d" SKIP_BUTTON +
   crystals when the step has a skip price, or the done mark; the window is one box shorter per
   missing step and rescaled; the reward strip in the order crystals, XP, gold, resources, items,
   centred when fewer than six), Show @0x35a9cc (a quick task shows its step's text line and
   runs its action instead), Update, Click, SetZ, Hide / ForceHide / TaskTimeout, OnHelp (closes,
   then OnHelpActual @0x35a648 from OnTweenOut), OnSubtaskSkip @0x35d174 (pays the crystals and
   completes the step: a crystal sink like speed-ups; the user is told in the part 2 summary and
   can veto), OnItem (reward popup), OnContinue, OnBack. TaskHolderWindow::OnTaskInfo opens it;
   Tasks::TaskTimeout closes it.
   Read from the asm: the status rules (r6 complete, [sp+0x12c] boss step = boss form and a
   kill-mob / kill-spawn step), the formats, the no-reward branch (the window loses the reward
   holder's height unless boss form).
   UNVERIFIED inside, each marked: the boss form (4g), OnHelpActual's calls into
   TaskSubtask::OnHelp / HelpWithMapSwitchTo (part 2b: it only prints "Help with subtask N"),
   SetTaskLocaterIcon and ExecuteFirstHelp (part 4), the special tasks' other windows (5),
   Locations::GetMapName (4f: another map's name is empty), GetUndecoratedString's task counts,
   sounds (6).
   Verified headless (temporary hook, removed): tapping a quest button opens the window for
   task 1281 (two steps, XP + gold) and 389 (one step, five rewards), sized and centred right;
   the action button closes it and reaches OnHelpActual with the right step.
   4d PART 2b DONE: the quest help. src/game/TasksHelp.cpp: TaskSubtask::OnHelp @0x2381d8 (its
   switch written from the real jump table at 0x238238: 0x44 entries, read with elfread),
   HelpWithLocation @0x22f53c, HelpWithMapSwitchTo @0x235964 (+ Continuation); Tasks.cpp:
   RunLastSubTaskHelp @0x23a968. TaskInfoWindow::OnHelpActual now runs them. Fully ported step
   types (all of the city's): build house (the unfinished building's arrow, else the shop on it
   with its arrow), build decoration (edit mode), upgrade house (speed-up arrow / the building's
   window with the upgrade arrow / the shop / the farm's upgrade button), increase land (camera
   to the area's middle x = 30 + (x + w/2) * 84, y = (y + h/2) * 21 - 90; the arrow's tap is
   Map::ClickToBuyArea) and any land (Map::GetNextAreaID), balance (NotEnoughWindow), order start /
   end / speed-up (the workshop's window with the arrow on the order, the shop when it is not
   built, the farm hover in the farm), build / upgrade any, start building, build and not finish,
   train in building (a non-barracks building's arrow), buy farm patch, get item / balance item
   for an item made in a building (NeedItemWindow::OnProduce) or lying on the ground, the
   pyramids' "HELP_FIND_RUBY", cast spell in the city, travel home ("TRAVEL_HOME" pop-up), "cannot
   travel" in combat. PORT: buy_real_money opens the exchange.
   IMPORTANT LESSON (2026-10-11): the scratch `raw` listing filter dropped standalone `goto x;` /
   `return x;` lines (they look like declarations). scratch raw.sh now keeps them. Parts 1 and 2a
   were re-read with it: two fixes (TaskInfoWindow::Click when hidden; the arena list's begin
   times). When writing a new filter, test it on a function with a switch first.
   UNVERIFIED in OnHelp, one comment per case: 4e decorations to use / click and FollowDecor; 4f
   persons, spawn points, portals, fog (ShowTaskLocaterTo); 4g enemies and spells; 5 the world
   map (every switch to another map only prints "the world map is not ported yet"), item shop,
   hire / craft / character / set / heal / level windows; 3f details of the farm view (the arrow
   on a growing patch, "WORK_RES_NO_SPACE"). Render::CenterOn is the instant one (the animated,
   zooming form is not ported). OBSERVED, to compare with the original if the user remembers:
   for an area at the map's edge the camera stops at the land's limit and the arrow (at the
   area's middle) can be just off screen.
   Verified headless (temporary hook, removed): "Order" of task 1281 (get 3 toolkits) opens the
   shop on the Smithy with the arrow on its cell; "Expand" moves the camera and places the arrow
   at (1240, 183) for area 3; HelpWithLocation at the hero shows the arrow and centres on him.
   4d part 3 DONE: windows/TaskCompleteWindow.cpp ("Quest complete!", Completed_win.xml): Tasks::Update
   now opens it for a finished quest with a reward (Complete(id) + Show) instead of completing
   at once; closing it (Ok / Back / outer tap) runs Tasks::CompleteTask(id, true, true, false),
   which drops the reward. The quest checks are blocked (Tasks::SetBlocked) while it is up.
   Reward strip order: crystals, XP, gold, resources 0..7, then items; item cells open the
   ItemHoverWindow. Also wired: TaskCompleteWindow::Hide in Map::SafeSave, main_Exit_Func and
   the background event; task 0x49f in TaskInfoWindow::OnContinue. PORT: no share ticks / wall
   post. UNVERIFIED left: the PvP-fight / queued-map-load / death-animation deferrals, the
   unlocked-spell CraftSuccessWindow and SetInfoWindow follow-ups (5), sounds, Map::Load's
   prologue (4f), a finished chapter quest still only returns (ChapterWindow is 4f).
   Test note: Tasks::Update runs once per real second - a headless hook must call
   Tasks::Update(true) after changing step counts. Verified headless (hook removed): task 1281
   finished -> window with XP 100 / Gold 300 -> Ok -> "Done task 1281", drops on the map, next
   quest appears; task 1758 (6 rewards, 2 items): arrows scroll, item popup opens.
   4d part 4a DONE: windows/TaskListWindow.cpp, the full quest list (Task_holder.xml): tabs All /
   Battle (show_in_battle) / City (is_city) / Completed (the ten completed last, stable-sorted by
   end time; no hints, chapters, dn_show, step type 0x25 quests unless is_dublicate_talk), eight
   slots in two columns on a ContentScroller (perLine 2), a slot opens TaskInfoWindow (a completed
   quick task only pops "TASK_COMPLETE_TEXT"). Opened by the bottom bar's "all quests" button:
   BattleBarWindow::ShowAllTasks @0x24b660 / HideAllTasks @0x249b90 are ported and called from
   TaskHolderWindow::UpdateTasks, ShopWindow Show / OnTweenOutInfo, BattleBarWindow Show / Hide;
   OnAllTasks @0x24a048 is the same code as TaskListWindow::Show. The button's text
   ("main_menu_button_tasks") is set in BattleBarWindow::Show (original: UpdateContents, 4g).
   The flag 0x617b90 that would show the quest line (g_slider) instead of the button is never set
   in 5.11. Read from the asm: slot looks (generic / blue / green / orange quick backgrounds),
   tab 5 hidden, the 4th icon's mask "quick_task_quest_holder_mask.png", Queue fields (update,
   back, show, hideOnOuterClick), the scroller's +0x78 is only the bar's holder.
   UNVERIFIED / left: the offline goblins' slots (GameState::GetOfflineGoblin: 4f; FindGoblin and
   BuyGoblin moved to 4f in port_plan), Locations::GetMapName (4f: another map's name is empty),
   RunLastHelpItem is ported but Map::Load does not call it yet (4f prologue, like
   Tasks::RunLastSubTaskHelp). The sort helpers are std::sort / std::stable_sort (util).
   Verified headless (hook removed): the "Quests" button shows bottom-left and opens the window;
   three open quests listed with pictures and "Found in: Kingdom"; the Completed tab lists two;
   tapping a slot closes the list and opens the quest window.
   4d part 4b DONE: the HUD's edge locators (HUDWindow.cpp): HideLocators @0x2c66bc,
   SetQuickLocatorForce, SetTaskLocaterIcon @0x2c680c (called by TaskInfoWindow::OnHelpActual),
   ShowTaskLocaterTo @0x2c814c, OnFindTask @0x2c7cb8, UpdatePlayerLocator @0x2c7220 and
   UpdateTaskLocator @0x2c6e04 (both now run in HUDWindow::Update); BuildingHovers::
   HijackArrowCallback @0x263260 and ExecuteArrowCallback @0x265468; GameState::Get /
   SetFeaturedTaskClickTime. In the home city neither locator ever shows (the hero's is hidden on
   map 0; the quest's only appears for a target under fog, HelpWithLocation's 4f branch), so they
   are untested beyond "nothing changed": re-test in 4f. UNVERIFIED inside: Fog::IsVisible (true),
   PlayerTopWindow::IsPlayerDialogVisible, the camera path (Map::AddCameraPoint) and the animated
   CenterOn (both the instant CenterOn), Update's once-a-second campaign-map locator, Map::Load /
   LoadShort calling HideLocators (4f prologue).
   MOVED in port_plan (2026-10-11), each with the reason on its rule:
   - every ::ExecuteFirstHelp (14) -> dead: the QA game bot (TutorialWindow::StartGameBot); only
     symbol-table entries, no caller / GOT / vtable entry. NOTE for such checks: an address in the
     .so file is the Ghidra address - 0x10000.
   - HelpWindow -> online (the platform web view of help pages, FileManager::BlogProcess).
   - BuildingHovers::HideWorldDialogByTask -> 4f (WorldHintHoverWindow).
   - CastleTopWindow's featured-quest button (5 functions) -> 5: every featured quest opens a
     milestone-5 window (pyramid exchange, chests, item shop sale, PvP). ASK the user then which
     promotions stay.
   - TaskListWindow::FindGoblin / BuyGoblin -> 4f (offline goblins).
   4d: EVERY FUNCTION IS PLACED (2026-10-11), part 5 (below) is still open. The last two windows
   turned out not to belong here:
   - LevelInfoWindow (29) -> 5. It is the levels-and-professions window (Window_lvl.xml: a
     vertical scroller of three rank holders, each with horizontal item / building strips, PvP
     statistics, five tabs from Professions::GetProfessionForTab). UpdateContents (2219 lines) and
     OnTab are built on Professions, Collections, PvP, ItemShopWindow: port it with Professions.
     Its setup code was read (Init, GeneralHolder / LevelHolder / UnlockedItemsHolder / ItemInfo /
     PvPStatisticsHolder LoadFrom): nothing written.
   - TaskQuickCompleteWindow (32) -> dead. It is an unused second quest-complete window (it
     completes the task and shows a "claim reward" strip), NOT a pay-crystals window as the plan
     had guessed: SetParameters has no caller, GOT or vtable entry and nothing else opens it.
   USER DECISION (2026-10-11): paying crystals to skip stays but "way more expensive just to deter
   people": a quest step's skip price is 5x the original ("port_skip_price_mult", TaskInfoWindow;
   docs/balance_plan.md section 5b).
   Small wiring done with it: TaskInfoWindow::BlockArrowLimitChangeOnce in the four hover
   windows, SetFeaturedTaskClickTime in TaskInfoWindow::Show.
   4d part 5 DONE (2026-10-11): the quest hooks inside windows ported earlier.
   - ShopWindow::OnBuy: a quest-locked building pops "HIRE_TROOPS_TASK_LOCK" with the quest's name
     (the first locked task of its chain when the quest is itself locked), else SHOP_UNLOCK_TASK;
     the cell's status shows SHOP_UNLOCK_TASK when the quest exists.
   - UpgradeBuilding (InfoHoverWindows.cpp): the upgrade lock names its quest, else UPGRADE_LOCKED.
   - LevelUpWindow: the "new tasks" cell (Tasks::HasTasksForLevel).
   - NeedItemWindow: SetItemsToBeRemoved fills icon / amount / NEED_ITEM_DESC_2 from the task's
     give_items[0]; OnProduce shows the open one of tasks 0x3f4 / 0x462 / 0x3f6 instead while the
     Smithy's first quests are open; away from home it calls Tasks::HelpWithMapSwitchTo(0, ...).
   - StringTable::GetUndecoratedString takes the task's three step amounts (`counts`); only
     TaskInfoWindow::UpdateDescription passes them, as in the original.
   - Tasks::CompleteTask: task 0x5b2 opens TaskInfoWindow on 0x5b3.
   - main Update: TaskInfoWindow::Update(dt) after Tasks::Update when not paused (the original
     calls it there besides the window queue's own call).
   Verified headless (hook removed): at level 6 the Carpenter's cell pops "Complete quest 'Juicy
   Carrots'"; the level-up window shows the "New Orders" cell. NOT exercised (no data / state for
   them in the test city): the upgrade lock's quest name, NeedItemWindow's task mode, a
   description with count slots, task 0x5b2.
   (ExchangeWindow's discount tasks 0x4d8 / 0x4da / 0x4db are the pyramid events: milestone 5.)
   4d IS CLOSED.
 - 4e part 1 (2026-10-11): the decoration's own state. game/MapDecor.cpp (new) holds Map::Decor's
   job side: worker (AssignWorker / RemoveWorker / WorkerIsWorking / WorkFinished), price
   (cost[11] parsed from the saved "TYPE=n|..." text; ReturnCost), action points (GetActionPoint /
   GetActionPointCount), trigger and block tiles, locks (IsLocked / GetRequiredItem), messages
   (GetMessage / GetHintMessage / GetBlockMessage / GetProgressMessage), animations' names, drops
   (CanDropItem), attackers, hit points (DecorData "hp"; 50 for the first decoration of a type
   without one, as the original), ranges and centre points. Decor::UpdateImage now sets the
   linear coordinates. LoadDecors registers decorations with an expression or hit points with the
   hovers too. Map lookups: GetDecorationThatIsReplacedWith, GetTriggeredDecoration,
   GetBlockingDecoration, GetDecorationWithDrop, GetDecorationWithAnyDrop, GetDecorationToClick
   (Tasks step 0x515 uses it). AIBaseState's empty job virtuals carry their addresses.
   UNVERIFIED inside (other milestones): chests (CHEST_DROP*, M5), craft nodes (CRAFT, M5),
   portals (Decor +0x90, 4f), offline goblins (4f).
   4e PLAN: part 2 = Decor::Update (0x13577c, 1500 lines: the finished job's rewards, tile
   triggers, traps), WorkStarted, UpdateOfflineState, particles, teleports, DecorAnimController,
   Store/RestoreDecorationInfo. Part 3 = AIPlayer's decoration actions (SearchDecoration,
   ClickedDecoration, AssignToJob, RemoveFromJob, SendGoblinToWork, ProcessAction's decoration
   half), Decor::OnClick, GetDecorationWithAnyInteraction, the hovers (FollowDecor,
   HideArrowOnDecor, ShowIconHover, ShowUseItemHover) and the 4e UNVERIFIED notes left in
   Tasks / TasksHelp / AIPlayer / BuildingHovers.
 - 4e part 2 (2026-10-11): what a decoration does. Decor::Update runs every frame from Map::Update
   and Map::OfflineUpdate: the animation sheet (DecorAnim = BaseAnimController<Map::Decor>, made by
   UpdateImage; DecorData frameTime from the child node's "fs"), the end of the job (Finish, a PORT
   split of Update: uses left, quest steps 0x22 / 0x23, START_QUEST or the taken REQ_UNLOCK_ITEM
   with its icon popup, DROP / DROP_AFTER_QUEST, PARTICLE_AFTER_INTERACT, REPLACE_OBJ, PORTAL,
   the three messages, OnInteract to the entities and the map), the hero on START_QUEST_TILE /
   END_QUEST_TILE / END_SUB_QUEST_TILE / TELEPORT_TILE, sprite visibility. Also WorkStarted (the
   price is paid, "-n name" popups), UpdateOfflineState, CreateParticles / RemoveParticles /
   UpdateDamageParticles / ReceiveDamage (DecorData "particle_points"), TeleportPlayerTile /
   TeleportAction / HandleTeleportTile, Map::StoreDecorationInfo / RestoreDecorationInfo (called
   in Map::Load), Map::RemoveHoldOfEntities + Building::RemoveHoldOfEntities (not called yet: the
   reload paths), BuildingHovers::ShowIconHover / FollowDecor / HideArrowOnDecor /
   AllowDecorClickInBattleOnce, Entity::SetEndWorkTS.
   UNVERIFIED inside, by milestone: traps (TRAP_*; 4g with Decor::Destroy / OnHitByEnemy), fog
   and map teleports and camera paths and mission drop multipliers (4f), chests and craft nodes
   (5), sounds (6). Render::CenterOn is instant in the port, so CenteringInProgress is false.
   Plan changes: DecorAnimController / BuildingAnimController are dead code; Decor::Destroy and
   OnHitByEnemy -> 4g; PortalAction -> 4f; PlayTeleportSound -> 6.
   Verified: builds, the city loads and runs 200 frames. NOT exercised: no decoration in the test
   city has a job, an animation sheet or an expression, so Update's job end, particles and
   teleports have not run yet (part 3 adds the way to start a job).
- 4e part 3 (2026-10-12): the hero and the taps. AIPlayer: ClickedDecoration (walks to the action
   point, action 5), AssignToJob (action 3), SearchDecoration (state 0x21, WorkStarted, the search
   animation), RemoveFromJob, SendGoblinToWork, GetBestDecorationActionPoint, TryToInterruptJob /
   StopMovement's worker removal, ProcessAction cases 3 (NeedItemWindow for the price or item, then
   the search), 4 (block message) and 5 (portal, goblin job, start / end quests, required item,
   message, the anvil's quests on map 0x5a). BuildingHovers::OnDecorClick's job half
   (NotEnoughWindow for the price, the hero away from home, a worker at home, the FREE_UP_WORKER
   popup), OnFreeWorkerAssignDecoration, Decor::OnClick, Decor::ShowUseItemHover,
   Map::GetDecorationWithAnyInteraction, GameState::GetGoblin, Entity::GetCurrentAnimation,
   Map::AssignEntities' decoration branch, the particles of OnSetup / OnTaskCompleted /
   OnSubTaskCompleted / Disable, the quest helper's arrows at decorations (steps 0x22 / 0x23, the
   ruin before tutorial 0x80, a decoration that drops the wanted item).
   Plan changes: CallbackAssignWorker is dead (never constructed); Map::RemoveBlockedTiles /
   RestoreBlockedTiles, TargetedAI::GetClosestReachableWPToDecor and AIPlayer::PlayBuffAnimation
   -> 4g.
   UNVERIFIED left inside 4e code, by milestone: several action points pick the first (4g:
   GetDistanceToTarget); combat taps, HealSquadWindow, traps (4g); portals, fog, map teleports,
   camera paths, offline goblins, the goblin popup's find / buy buttons, the use-item hover type
   in UpdateHovers (4f); world dialog hides (4h); chests, craft nodes, collections (5); sounds (6).
   Verified: builds; the city loads, saves, reloads and runs; a tap on the map still works;
   inventory shows no 4e function left. NOT exercised: the test city has no decoration with a job
   or an expression, so the job flow itself (tap, walk, search, rewards) first runs on a campaign
   map (4f) or in the tutorial (4h). Test it there before trusting it.
   4e IS CLOSED. NEXT: 4f (campaign maps).
   4f (2026-10-12/13), parts 1-4 (3f8d6b9 .. 177df70), then 5 and 6:
   - part 1: src/game/Locations.* (locations.xml: campaigns, missions, their maps; the city is
     location 0x400), GameState's map / campaign state (visit, start, end times, completions,
     monster kills, campaign status, map versions), EntityManager::GetEntityByEnemyClassID, and
     every hook that waited for them (Tasks campaign / mission lookups, mastery and campaign-percent
     steps, quest windows' map names, the HUD quest list's campaign percent and mission timer,
     mission multipliers on decoration drops, Override kinds 5 and 0x11).
   - part 2: src/game/SpawnPoint.h (SpawnPoint 0x54, Portal 0x20), src/game/MapSpawn.cpp (load /
     save of chunks 2/9 + 0x14/0x25/0x28/0x42/0x44/0x45 and 3/10, lookups, Store/Restore
     SpawnpointInfo), Entity's spawn point / patrol / talk / hint helpers, EntityManager::
     SpawnEntityAt(SpawnPoint*) / GetEntityBySpawnPoint / GetEntityWithGUID / GetEntityWithTalkTask
     / GetEntityWithDrop / RemoveEntitiesByID, UpdateSpawns' spawn loop. Entity +0x174/+0x178 are
     talkTask / specialTalk (were `selected` / f178).
   - part 3: the map switch (Map::EnqueueLoadMap, Map::Update's queued branch, Map::Load's
     leave-map start, task / help steps and map name), campaign maps' walkable grid (Patch::tiles
     from the mask, CreateRoadAI), portals (Decor::portal, PortalAction, AIPlayer::ClickedPortal,
     ProcessAction case 1). Map::Load now takes (block, time, newGame, fromCache).
   - part 4: src/game/Fog.* (blocks, quad tree, rim sprites, load / save chunk 0x30, Open, Update),
     Map's fog list (chunk 0x17) and its wrappers, Fog::IsVisible in buildings, decorations, the
     quest helper and the HUD locator, AIPlayer::Update's RemoveFog, the map version update
     (UpdateBattleMap; UpdateCityMap is 4h).
   Verified so far: the city loads, saves (with fog) and reloads; campaign map 14 loads with its
   NPCs, monsters, fire particles and fog; the hero walks through a portal and map 15 loads (test
   hook, removed). Facts learned: a campaign map's walkable tiles are small islands until quests
   remove the blocking decorations; `--map N` loads a map file directly; a headless run only saves
   with `--save`; tools/gotslot.py names a GOT slot from a function's literal.
   UNVERIFIED left in these parts: boss / combat branches of UpdateSpawns and the map switch (4g),
   GlobalMapWindow and its sepia effect, the loading screen's trigger, PvP shadows (5), the camera
   points (ClearCameraPoints etc.: still 4f), HideWorldDialog (4f windows), tutorial task fixes and
   UpdateCityMap (4h), sounds (6).
   - part 5 (693b4bf): src/game/AIEnemy.cpp (AI state 9: the aggro zone from SpawnPoint::aggroRange;
     CheckAggro, which starts the fight, is 4g), talking (AIPlayer::ClickedEntity's talk walk,
     ProcessAction case 2 with NeedItemWindow, Entity::OnTalkTo), GameState's offline goblins and
     buildings, TaskListWindow's goblin rows + FindGoblin / BuyGoblin (the goblin popup's buttons),
     src/game/MapCamera.cpp (AddCameraPoint / AddCameraPath / shake / cinematic bars / UpdateCamera;
     CAM, CAM_AFTER_*, SHAKE_AFTER_* expressions; the view jumps where the original glides).
   - part 6a-6d (04edfe1 .. 3a9dbb3): TalkHoverWindow (hover type 0xc) and the hover decisions for
     talk tasks, the guide's special talk and decoration jobs; src/hud/WorldHintHoverWindow.cpp +
     BuildingHovers::ShowWorldDialog / HideWorldDialog / HideWorldDialogByTask (quest dialogs said
     by their givers in Tasks::CompleteTask / Update); HUDWindow::UpdateScreenDarkening (every
     open window now darkens the screen behind it, as the original), HardMapReset / MapReset /
     SoftMapReset, HideMapName / HideBlack, the campaign map's once-a-second quest locator;
     src/windows/ChapterWindow.cpp; Tasks::Update gives NPCs their talk tasks.
   Plan changes in 4f (tools/port_plan.txt): dead: Locations::ReportLocationTaskState, the map's
   own person list (Map::Person*, LoadPersonList ...), PersonHoverWindow (created, never shown);
   online: GiveAllChaptersTillCompleted, LoadEntitiesFromCopy; 4g: AIEnemy::CheckAggro,
   EventManager, Entity::GetClosestWPTo, PlayerHintHoverWindow (PlayerTopWindow's dialog),
   TapHoverWindow (the belt's); 5: CmdOpenMap, CampaignCompleteWindow, CampaignPerfectedWindow,
   LoadingScreenWindow, FoundItemsWindow::SetParameters(location) (all only reachable from the
   global map).
   Verified: campaign map 14 with fog, NPCs, the world dialog (test hook), the chapter banner (test
   hook), the darkening behind the quest list, city save / reload. A hole found from 4e:
   BuildProgressHoverWindow's decoration branch (the progress box over a decoration being searched:
   HoverWindows.cpp "decoration jobs" notes) is still UNVERIFIED.
   INCIDENT (2026-10-12): one test run was started without --storage (a mistaken `--help` call), so
   the game loaded the user's real save and re-saved it at 13:19 with the build of that moment
   (spawn points now saved with it; task 1182 completed on load). A copy of that save loads fine;
   a backup copy is in the session scratch dir (realcopy_keep). Always pass --storage AND
   --screenshot; there is no --help.
   - close-out: ChoosePathWindow, CampaignInfoWindow and HintHoverWindow moved to milestone 5: every
     ChoosePathWindow answer ends in GlobalMapWindow (Show / ArrowAtMap / SwitchMapAnimated) and
     CampaignInfoWindow's Start is GlobalMapWindow::SwitchMap, so they are unusable before the
     global map. Until then travel between maps is by portals only, and the guide's "where to
     next" talk (special talk 0x1d7) does nothing. Leftover "4f" notes in older code were resolved:
     done now (Tasks' spawn point lookups, help arrows at portals / talk persons / enemies that
     drop an item, Decor::TeleportPlayerMap, AIWarrior::StopMovement's patrol end) or relabelled
     to their real milestone (4g: PlayerTopWindow's dialog, retreat, corpses; 4h: UpdateCityMap's
     patch blocks; 5: map loot lists, mission backgrounds and timer, the emperor's map).
   NOT VERIFIED in 4f: a quest played through on a campaign map (talk -> dialog -> task done ->
   blocked path opens -> portal), which needs the new game start (4h) or combat (4g) to reach
   naturally; reloading a campaign map from the save (the port still starts in the city whatever
   map was saved: LoadSavedGame, 4h; a map switch back to a visited map uses the same code but was
   not run); camera paths and shakes (no test map triggered one); enemy zones (no fight yet).
   4f IS CLOSED (inventory: 0 functions). NEXT: 4g (combat as used by the campaign).

**4g DONE (combat as used by the campaign; closed 2026-10-13, inventory: 0 functions, no "milestone 4g" note left in src).**
   USER DECISION (death / revive, 2026-10-13; also in memory rtk-freemium-removal): health comes
   back by itself, a full bar in at most 4 minutes; "Heal now!" costs earned crystals; potions heal.
   Implemented as GameState::HpRegenSeconds (min(original, 240 s / HpMax)), used everywhere.
   Parts:
   1 fight core: Combat.h/.cpp (BaseCombat, AggroCombat, RangeAggroCombat), AICombat.cpp (AIWarrior
     StartedAttack / Attack / Hit / Block / Death / ProjectileLanded, combat timers, AIEnemy::
     CheckAggro), EntityCombat.cpp (OnHit, Hit, WillDie, OnDying, OnDeath with drops / gold / xp /
     quest steps / broken gear, orb bonuses), BRand.h, GameState SetCombat / EndActiveCombat /
     UpdateHPRegen, the fight's update in main.cpp Tick, AIPlayer::ClickedEntity.
   2 arrows: Projectile.h/.cpp, projectiles.xml.
   3 PlayerTopWindow::Update (live bars, hurt portrait, "+1 HP" popup, items flying in).
   4 BattleBarWindow (army bar slots, health fill, red flash, soldier card).
   5 HealthbarTinyHoverWindow and its rule in BuildingHovers::Update; HUDWindow::OnDamage.
   6 windows/HealSquadWindow.cpp (the Heal Hero window).
   7 BeltBarWindow core (slots, item card, OnUseSlot, OnActivateSlot, UpdateBeltItems).
   8 TargetedAI::GetDistanceToTarget / SetBlockedPathTarget, Map::RemoveBlockedTiles /
     RestoreBlockedTiles, AIPlayer::Retreat, GetBestDecorationActionPoint.
   9 the hero's speech bubble: PlayerHintHoverWindow, PlayerTopWindow::SetParameters / HideDialog /
     ShowArrowAndLock / IsPlayerDialogVisible; said by Tasks::CompleteTask and Tasks::Update for a
     quest without a giver (only with a hero on the map); consulted by the HUD locators, quest
     banners, LevelUpWindow, the window queue, the settings key and the language question; closed
     by HideWorldDialog and EnterFarm. It is updated like every WindowQueue (ProcessUpdate), not by
     PlayerTopWindow::Update. The window queue and TaskCompleteWindow also wait for
     Entity::IsDeathAnimationRunning.
   10 traps (Decor::TrapDamage / TrapPattern: Update's TRAP_DAMAGE, TRAP_PATTERN, TRAP_ANIMATED
     parts; used on maps 71, 82, 83, 84, 87, 92, 96, 121, 132), the corpse search's result
     (AIPlayer::ProcessAction case 7: corpse_drop's second number is the chance in percent), quest
     help for kill / kill-all / kill-class / tame-class / heal-hero steps, HealSquadWindow::ZRange,
     HealSquadWindow::Show on taps while the hero is dead, CharacterInfoWindow::UpdateContents
     from the squad and its belt refresh, EndCombat from the map resets and the camera path,
     GameState::EndActiveCombat at exit.
   FIXED WHILE TESTING: HealSquadWindow::SetZ returned while shown instead of while hidden (the
   window sat behind the dark cover); BaseCombat::Pause ended the turn for a null entity instead of
   a non-null one (the hero's death now ends the turn, as the asm at 0x10c3f8 says).
   PLAN CHANGES (tools/port_plan.txt). Dead in 5.11: the siege system (CombatManager, EasyCombat,
   EasyAI, SiegeCombat, AISiegeAttacker, AIWarrior_v2, EventManager, the "Squad" class at 0x225de0,
   tower attacks, Decor::Destroy, GameState::g_Combat's branches, Entity::CalcDamage /
   ReceiveDamage / Die), HealthbarHoverWindow, TapHoverWindow, TargetedAI::GetClosestReachableWPTo*,
   HealSquadWindow::SortItemsByMetaSort. To milestone 5: AIWarrior::Magic* (spells), BaseCombat::
   SwitchTo{Boss,Arena,PvPArena}Fight and everything "boss" (ResetBossFightsOnMap, the victory
   pose flag 0x6138cc, TaskInfoWindow's boss form), BottomBattleWindow (rage bar), PlayerBattleInOut
   Window (PvP), HealHoverWindow + BattleBarWindow::ShowHealSoldier, the belt's crafting /
   inventory (OnTweenIn's flag 0x617dbc) / buff / MoveIntoBelt parts, Shared::BuffHolder(Manager),
   EndscreenProgressBar, ProgressBarLarge, WarriorSlotInfo, PlayerWeakWindow (ASK THE USER FIRST:
   it advertises chests), Cmd* console commands, taming (the net check in quest help). To 4h:
   Shared::PlayerHolder (the heal window shows a plain bald figure until then). To 6:
   Render::CreateCustomTexture, all fight sounds (TRAP_SOUND too).
   PORT guard until milestone 5: a boss neither attacks nor can be attacked (AIEnemy::CheckAggro
   and AIPlayer::ClickedEntity return for Entity::IsBoss).
   VERIFIED (screenshots, scratch storage, map 15 unless said): a fight by tapping the monster
   and one started by its aggro range; the hero's death -> Heal Hero window; "Heal now!" (50 -> 40
   crystals, full health, the arrow at the attacker, its tap resumes the fight to the kill);
   "Flee to castle" (fight ended, back in the city); a potion drunk from the belt (3 -> 4 HP, 3 ->
   2 potions) and from the heal window while dead (0 -> 1 HP); an attack orb on the belt (card
   with Deactivate / Remove, +1 damage, the charge bar drains); the blocked-way hint ("We need to
   talk to the hostage" after walking up to the blockage); the speech bubble with the real quest
   line and its Next; a trap on map 71 (-1 HP, the hero steps aside); a spawned skeleton archer
   (id 46) shooting at point-blank range (-2 per shot).
   NOT VERIFIED: an arrow's flight over a distance (the melee hero always walks up to an archer;
   needs own archers, milestone 5 hiring); a soldier's death (the lone monsters of the test maps
   always pick the hero); AIPlayer::Retreat in its real use (its only callers are the boss quest
   window's buttons, milestone 5; called by hand in a fight it walks off and the fight goes on).
   SEEN, NOT 4g: (a) "profession_spawn" decorations (id 1507, CRAFT=...: the crafting spots of
   milestone 5) are drawn as pink tiles on campaign maps; milestone 5 must find how the original
   hides or replaces them. (b) In a fast headless run a dead monster's health bar stays until the
   hover rules run again (once per real second, as on the original): not a bug. (c) Fleeing in the
   test save meets "Chapter 2" in the city and the bars come on top of it (the original's Show
   functions do the same; a real save has seen that window long before). (d) The PORT test city's
   hero has 10 HP with a maximum of 9.
   HOW TO TEST A FIGHT: `python3 tools/test_hook.py on tools/test_hook.txt` puts a test loop into
   src/main.cpp (take it out with `off` before committing). It runs when RTK_T is set: RTK_T=fight
   (tap the nearest monster at frame 30), aggro (walk to RTK_DX, RTK_DY beside it), list (print
   the map's entities), trap (stand on trap RTK_N), bubble, none. Extras: RTK_HP, RTK_SHP
   (soldiers), RTK_LEVEL, RTK_CRYSTALS, RTK_ITEM (3 of it, bound to the belt), RTK_FOE (entity
   id), RTK_CLICK=frame:x:y,... (screen taps), RTK_TILE=frame:x:y (tile tap), RTK_RETREAT=frame,
   RTK_SLEEP=frame (2 real seconds). Example:
   RTK_T=fight RTK_HP=1 RTK_CRYSTALS=50 RTK_CLICK=200:735:352 ./build/rtk --root .. --storage
   <scratch> --screenshot out.png --second-tutorial 256 --map 15 --frames 300
   ALWAYS --storage and --screenshot; there is no --help. Level-2 items (potion 354, orb 133) need
   RTK_LEVEL=3. Heal window at 1280x800: Heal now 735,352; Flee 851,352; item cells from 720,500;
   belt slots 1185,765 and 1245,765.
   WORKFLOW REMINDERS: read one function with
   awk '/^\/\/ ==== /{ if (p) exit; p=($3==ADDR) } p' out/decomp.c | tools/picsym.py; for "who
   uses this global" run picsym once over the whole of out/decomp.c into a scratch file (18 s) and
   grep it; check a doubtful branch in out/asm_all.txt (both bugs above were inverted conditions);
   a class nobody constructs is dead: check before porting.
   NEXT: 4h (the new game and the opening tutorial).

**4h IN PROGRESS - RESUME HERE (updated 2026-10-18).**
   STATE 2026-10-18 (commit e5facf6 "4h part 7", and part 6 2407fd1 before it): everything in 4h is
   ported except ONE item. The ambush (PvP arena fight) and the world map are done and verified;
   `inventory.py --functions 4h` and `--unplanned` are empty; no "milestone 4" note is left in src.
   The city tutorial was played in the test loop from step 0x1b to 0x61 (house from the ruin,
   taxes, the tree, level 2, the farm: plant / speed up / harvest / food, the bakery order and
   hurry, Chapter 3, the barracks built and hurried, quest "Training the Troops"). Fixed on the
   way (all were notes or missing code): Render::CenterOn's 0.4 s glide (Render::UpdateCamera) and
   every call site matched; AIWorker::WalkCompleted steps 0x1c/0x23/0x26/0x2b; StartWorking 0x5d;
   AssignToJob(Decor) 0x1e and its action point; AIGoblin's decoration job (step 5, RemoveFromJob,
   AssignToJob) and step 0x30; BottomCityWindow 0x5a/0x5b; BuildProgressHoverWindow's decoration
   branch (SetDecoration's auto speed-up 0x1e/0x3a, Update, GetSpeedUpCost, GetRemainingTime);
   BuildingHovers::MoveRectIntoView (was wrongly "dead"); TaxesHoverWindow closes the dialog;
   CastleTopWindow OnReturn (the compass: world map), OnFindPlayer (HUD locator), Update (button
   states, tutorial arrows 0x67 / 0x80+0x81 / type-1 0x37), Click, Show conditions;
   HUDWindow::OnFindTask camera paths; ExchangeWindow's discount tasks 0x4d8/0x4da/0x4db;
   BeltBarWindow second-tutorial steps 0x95/0x8b/0xa3/0x86; LevelUpWindow, PopupSelectionWindow,
   MapMovement / main arrow auto-hide; Decor::Contains; GetGoldEarned (always 0 in 5.11).
   TEST NOTES: jobs, hovers (UpdateHovers once a second), the farm speed-up and patch boxes run on
   wall-clock seconds, so the test loop needs RTK_SLEEPS around every wait; random drops are taken
   with RTK_COLLECT. "Blooming 366%" in a sped-up patch box is the fast test frames, not a bug.
   The city run's taps (from a save at step 0x1b, made by the ambush run with
   scratch clicks_amb.txt): 20:640:700, 160:975:420 (ruin), 450:975:372 (taxes), collect 540,
   700:808:520 (tree), 920:636:668 (level-up OK), 1000:715:364 (Next), 1120:640:440 (farm),
   1540:745:735 (speed up), 1700:780:275 (crop arrow), collect 1800, 1880:533:735 (Return),
   1960:515:448 (baker), 2040:740:490 (bakery), 2100:745:398 (order), 2160:870:400 (hurry arrow),
   collect 2220, 2300:640:700 (Chapter 3), 2420:875:365 (barracks hurry), 2500:640:645 (quest OK);
   sleeps at about every 40-60 frames from 400 on.
   THE ONE ITEM LEFT (user 2026-10-18: "port it now", pulled from 5 into 4h): HireTroopsWindow
   (0x2cdd30..0x2dae58, 52 functions). Step 0x61 "train your first warrior" opens it from the
   barracks (BuildingHovers::Click's kHireTroops, TaskInfoWindow::OnHelpActual, HoverWindows' cls
   0xc: ShowForBuilding(tutorial 0x61 ? 0x8f : id)); its Show/OnTweenIn/Click/OnTrain carry the
   steps 0x61 -> 0x65; then AIWarrior::WalkCompleted 0x65, TaskCompleteWindow 0x65 -> 0x66, the
   envoy 0x66 -> 0x67, the compass (CastleTopWindow, ported) -> the world map -> 0x80. Sizes:
   Init 502 decompiled lines, OnTrain 463, WarriorInfo::LoadFrom 175 (0x3e4-byte entries, 6 tabs),
   UpdateContents 0x2d05e4..0x2d7ae8 (30 KB; Ghidra gave 2 lines: read the asm). After it: play
   0x61..0x80 in the test loop, check a save round trip on a copy, close 4h (roadmap [x], STATUS
   entry), commit.
   DONE AND COMMITTED:
   part 1: the new game (LoadSavedGame's branch; "--map N" keeps the old test start past the
     tutorial), hud/TutorialWindow.cpp (Update steps 0..0x17, UpdateCity 0x18..0x66, UpdateSecond;
     TutorialSender's bookkeeping), windows/SelectHQWindow.cpp (name and gender; figures are bare
     bodies), game/PlayerEquipmentInfo.*, Shared::PlayerHolder::LoadFrom (hud/PlayerHolder.cpp),
     shader type 10 (grey fade), Render::GetSymbolCountInWidth, EntityManager::GetEntityByClass,
     Entity::SwitchEntityData, OnTutorialFarmerWomanAction, BottomFarmWindow::GetPlantButton.
     TOOL: tools/ghidra_scripts/SwitchDecomp.java restores ARM jump tables Ghidra lost
     (`addls pc,pc,rN,lsl #2`): args <out.c> <entry> <size> (<switch insn> <cases>)...; output kept
     in out/switch/. Run: tools/ghidra_headless.sh "$PWD/ghidra" rtk511 -process libkingdom.so
     -readOnly -noanalysis -scriptPath "$PWD/tools/ghidra_scripts" -postScript SwitchDecomp.java ...
   part 2: the tutorial hooks left as notes in older code: BuildingHovers (UpdateTutorialDrops =
     Update's first part, CollectOne, the steps on a tapped drop, ClickOnArrow in Click, the arrow
     following an entity, OnCollect's chest loot / net / step 0x17, ItemDrop::Collect and Animate,
     DropItem / DropResource / DropFarmFood locks, UnregisterDecoration), the hover windows (taxes
     0x21, ready bubble 0x55, build progress 0x53/0x54/0x5e, placement 0x5c, factory 0x53/0x54),
     Building's farm step 0x45, Map::Update's farm arrow 0x3a, AIPlayer's taming click.
   part 4: the taming lesson (hud/BottomBattleWindow.cpp's net, GameState::HireSoldier, OnFeed).
   part 5: windows/GlobalMapWindow.cpp (the whole world map: locations, nodes, scrolling and fling,
     travel, arrows, sepia frames, the sultan's caravan; its milestone-5 windows are notes),
     windows/BossIntroWindow.cpp (the ambush banner; reputation screens are milestone 5),
     Shared::EndscreenProgressBar, FirstCityEnterEffect::Run (in Map.cpp), Map::Update's queued load
     (world map on the way home, sepia, Hide), AIPlayer's portal (Show + ShowTutorialAnimation),
     Render::Update / minLayer / clearColor / CreateScreenTexture / shader 9 (textVS + sepiaPS),
     Timer::AddDeltaTimeCompensation, SetInfoWindow::GetPlayerItemOrBetter (windows/SetInfoWindow.cpp;
     Tasks uses it), GameState::ResetBossFightsOnMap / GetEventCompletionCount.
     VERIFIED: the exit after the boar opens the world map; the marker walks from the start node
     and "Ambush!" shows halfway (the view follows the marker; the map image has its own frame).
   USER DECISIONS (2026-10-14): the world map is 4h (its info windows stay 5). The ambush is a
     turn-based PvP arena fight against a fake rival (tasks 0x772 / 0x773) and is ported in 4h too:
     BossIntroWindow's fade-out starts it (PlayerCompetitionWindow's player -1, PvP::GetPlayerInfoByGID,
     the rival's gear from items of type 0x11 by slot, a hero 0xc / 0xd, SetPvPTutorial, AggroCombat
     -> ForceBossFight(6, 0x91)); then PvPArenaCombat / ArenaCombat (turns), ArenaTurnWindow,
     PlayerBattleInOutWindow, PvP::CreatePvPTutorialPlayer(s) (main_Loop_Init). NEXT: port that.
   VERIFIED (screenshots, scratch storage): new game -> Chapter 1 -> SelectHQWindow (gender
     greys the other figure, the default name follows) -> Stoutbeard's greeting with the chosen
     name -> arrow at the signpost -> map 13 -> the wolf fight -> the miner's thanks -> the chest
     opens and drops its loot with the arrow on the first piece (step 0x15) -> the loot picked
     up -> step 0x17, "Look, a wild boar!" with Next.
   TO DO, in this order:
   1. DONE: the chest's loot is picked up (the test loop's RTK_COLLECT=frames calls
      BuildingHovers::CollectOne(0), as the arrow's tap does); the net (0x254) gives step 0x17 and
      the hero says "Look, a wild boar! ..." (DESC1911) with the resource bar and the belt shown.
   2. THE TAMING LESSON (step 0x17, on the same map 0xd; the boar is on tile (7, 0x10), spawn
      point 0x1e). Flow on the original: Next -> arrow at the boar -> fight -> as it dies
      BottomBattleWindow::EnableNet(spawn) -> "TAME_TUT2" in the hero's bubble and the arrow at
      the bar's centre button -> OnCenter throws the net (a sprite flying from the hero to the
      boar) -> BottomBattleWindow::Update lands it: in the tutorial always a catch -> the boar
      joins the army (GameState::HireSoldier) -> arrow at the portal decoration (10, 0x12), fog
      opened there, task 0x771 done -> the city (step 0x18). ItemHoverWindow::OnFeed (feeding it)
      has its own note.
      WHAT TO PORT (new file src/hud/BottomBattleWindow.cpp, replacing the inline stubs in
      hud/HUD.h; the rest of the bar stays milestone 5): Init 0x25d4a8 (Rage_bar.xml, scale
      GetHudScaleFactor; offsets 0x60f110 = 0x17 and 0x60f10c = 0, both times the root's scale;
      usesZRange false; MovementEffect with OnTweenOut = hide; ButtonFuryLarge x3:
      button_fury_large_01 / _02 (OnRight) / _03 (OnCenter); fn.zRange ReturnSmallZRange,
      fn.update, fn.show), ButtonFuryLarge::LoadFrom 0x259748 (root "%s", fury_button_active_state,
      button_bg_orange / _blue / _green_circle / _red, icon_60 with takesZ) and SetColor(i) (only
      background i shows), Show 0x25cfd8 (only from tutorial step 0x17 on; slides up from the
      bottom, centred), Hide 0x259338, SetZ, IsVisible, Click 0x259564 (the spell-drag part is
      milestone 5; the rest is root->Click), EnableNet 0x258f30, TameNext, GetNetTargetSpawnID,
      OnCenter 0x2588b4 (its net branch), and of Update 0x259c70: the net timer (0x61842c,
      frozen in the tame tutorial; target 0x1f is dropped there), the centre button's net look
      (blue, pulsing glow 0.5 + cos(t * 8) * 0.6, the best net's icon, text_stance "TAME_HINT"),
      the flying net (0x618414 sprite, shader 8; t 0x618418 from 0 to 1 at 1.5 per second; size
      (0.2 + 0.8 t) of the icon; from the hero's (x, y + idle height * -0.5) to the monster) and
      the catch (chance: the net's meta 0x25 times the tamed EntityData's +0x158, out of 10000;
      Disappear, RemoveItem(net), CompleteSubtask(0x43, enemy class), HireSoldier or the item
      whose meta 0x27 names the tamed entity when the reserve is full, else "effect_text_resist").
      EnableNet: target = spawn only if EntityFactory::GetTamedEntityForID(entity id) exists (the
      EntityData of class 0x11 with the same enemy class, +0x28) and its unlock quest (+0x12c) is
      not a locked task; timer = 0.5 + rand48 % 1000 / 1000 * 0.5; for spawn 0x1e / 0x6e9 first
      the camera to (616, 260), the bubble, the arrow at the centre button with OnCenter as its
      tap. Missing helpers: EntityFactory::GetTamedEntityForID 0x160720, GameState::HireSoldier
      0x19a15c (1124 bytes: the hire-time and hire-count maps 0x6132fc / 0x613314 / 0x61332c,
      then the free slot or the reserve; milestone 5 planned it with hiring).
   3. BeltBarWindow's second-tutorial notes (2), HealSquadWindow's figure.
   4. Shared::PlayerHolder::PlacePlayerEquipment / PlaceItemInHolder / PlaceObjectInHolder
      (0x34572c, 0x345058, 0x344d78: read with care, Ghidra mixes their float and int arguments;
      check each call's registers in out/asm_all.txt).
   5. ReloadSavedGame 0x1889a0, HUDWindow::GameReset 0x2c8e44 / GameReload 0x2c989c (SettingsWindow's
      restart), GameState::RemoveWorkersFromOrders 0x190c74. To milestone 5 in the plan:
      FirstCityEnterEffect::Run (global map animation), CmdReloadGame (console). Dead (no caller):
      TutorialWindow::StartTutorialSkip / StartGameBot / GetResource / BotShouldTame / Show and so
      SkipTutorialStep's body, SelectHQWindow::OnContinue / SetOnContinue.
   6. The city tutorial (steps 0x18..0x66) has not been run yet: play it in the test loop and fix
      what breaks. Tablet attention rings (BuildingHovers notes): check whether a desktop build
      ever shows them, else re-label.
   7. `python3 tools/inventory.py --functions 4h` and `--unplanned` empty; the notes gone; this
      block replaced by the finished entry; roadmap line; commit; short update to the user.
   TEST LOOP: tools/test_hook.py on|off tools/test_hook.txt (see the 4g entry). New: prints
   tut=<tutorial>/<secondTutorial>; RTK_NOCLOSE=1 (no automatic window-closing taps),
   RTK_SLEEPS=f1,f2,... (1.1 real seconds each: jobs run on real time). The new game's taps so
   far (1280x800): RTK_CLICK=40:640:400,100:640:630,200:758:418,240:725:235,340:555:215,
   560:842:262,640:768:215 with RTK_SLEEPS=720,730,740,750 reaches step 0x15 at frame 760;
   RTK_COLLECT=800,830,860,890 then gives step 0x17 at frame 900. RTK_COLLECT=frames is new.
   Input test aids (from the bug hunt): RTK_TRACE_INPUT=1 (prints where each press/release lands) and --move X Y. Also fixed: the test
   city's GameState::tutorial is 0x80 (finished opening tutorial; was 0x100). Saves made by older
   builds carry 0x100.
   4a.1 DONE: squads and the army. src/game/Squad.* (BaseSquad/PlayerSquad/EnemySquad in vtable order:
   formations table 0x579d8c, SpawnAt/SpawnSquadAt, MoveTo/MoveSquadTo, ready flags, heal/rearrange/
   remove with SoldierPool reserves), src/game/SoldierSlots.* (SoldierSlots + SoldierPool), the
   minimal BaseCombat interface (src/game/Combat.h; GameState::GetActiveCombat is null until 4g),
   Entity squad/stat parts (CreateSquad +0xcc, OnAdded/OnRemovedFromSquad with the 0x6119cc
   destructor flag, SetAP/AddAP/SetHpMax/override attack-defense, AddHP + the HP over-limit,
   IsEliteSoldier, speed run, UpdateSafePos, SetSpawnPoint stub), EntityManager DestroyEntity's
   soldier part/AssignSoldiersToPlayerSquad/GetPlayerAndSoldiers, AI::GetRandomWaypointInRange,
   GameState LoadEntities/SaveEntities with the hero/soldier/squad branches (g_keptEntities is
   gone), GetSoldierSlotCount, the reserve slots in SoldierPool (Reset also sets reserve_slot_max),
   Map::UpdateSpawns (the player's part: city default tile (0x1b, 0xd), facing 1; spawn points 4f),
   Map::SpawnEntities (player waypoint weight 5, squad SpawnAt; boss combats 4g), Map::Load's camera
   (on the player, then the city's tile (0x1e, 0xc)); main only overrides it with --camera.
   The PORT test path (no save) now makes the hero 0xc with its squad and soldier 0x1a2 as the
   new game does. Verified headless: hero and soldier stand by the castle; save -> reload -> save
   is byte-identical from the second reload on (the first moves the new hero's HP 10 over HpMax 9
   into the over-limit: chunk 0x4f's HP goes 10 -> 9, as on the original); the soldier reloads in
   SoldierSlots and the squad. Old port saves have no hero (SpawnEntities skips: PORT guard).
   4a.2 DONE (4a complete): the hero walks where you tap and the army follows. src/game/AIPlayer.cpp:
   AIWarrior (soldiers: speed 239.4 / 180, "run", SetReady on arrival, city training at a workplace
   with the random swing/block/rest, AssignToJob/RemoveFromJob, StopMovement) and AIPlayer (Update with
   the 0.25 s pass that fades decorations in front of the squad in combat, ClickedTile with every
   original gate: tutorial >= 8, step 0x17 needs task 0x771, the city only at secondTutorial 0x100,
   not over the shop/edit modes, only owned land and tiles of weight <= 1; ClickedEntity's non-combat
   path: a reachable character gets Entity::OnClick and the blocked marker, an unreachable one moves the
   squad there; WalkCompleted (arrival direction, someone on the tile steps aside, ProcessAction,
   SetReady), UnableToWalk, TryToInterruptJob, Reset, StopMovement, RemoveActionMarker, the tutorial
   0xb/0xd arrows; ProcessAction's skeleton: cases by milestone, 6 -> 7 search ported). The tap
   markers ("goto", "blocked"; 1.5 s, layer 0xc), IconManager's animated icons (FUN_002dc20c frame
   chains, also the sand clock and "!" now animate) and the renderer's animated-sprite registry
   (Render::Register/UnregisterAnimatedSprite, UpdateAnimatedSprites in the tick before drawing,
   Texture::next). main routes a desktop tap after ClickToBuyArea to EntityManager::OnClick (not while
   dragging), with the original's "clicked tile at" print. Also EntityManager::OnClick/
   GetNonPlayerEntityAtXY, AI::GetNearestWPFromNonWP, WindowManager::GetMousePosition, HUDWindow::
   Update's no-player return. PORT test aid: --second-tutorial N (the test city keeps 0x81, where the
   original ignores city taps; pass 256 to walk).
   Verified headless: a grass tap shows the animated goto marker, hero and soldier run there ("run",
   the squad's MoveTo) and stop on/next to the tile idle; at 0x81 taps are ignored; taps on hovers stay
   with BuildingHovers; save -> reload -> save byte-identical (the city always respawns the hero at
   (0x1b, 0xd), as the original). Left for later steps (marked UNVERIFIED): combat (4g), talk/portal/
   decoration actions (4c/4e/4f), the blocked-decoration path (4e), sounds (6).
   Testing note: in zsh an unquoted "$c" holding "900 500" is one argument (--click then reads y = 0).
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
2. 3f farms: done (3f.1-3f.5).
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
- Plan coverage: docs/port_inventory.md places every class in a milestone and tools/port_plan.txt
  every function in a step. After a step: `python3 tools/inventory.py --unplanned` (must print
  nothing) and `--steps`; when a step's functions are done its count drops. New work found while
  porting gets a rule in port_plan.txt.
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

## Project decisions (user, 2026-10-07) — apply without asking again
- Seasonal events (Tags such as xmas_2012, thanksgiving_2012) switch on by the real calendar date
  (the dates still to be chosen per tag when events are ported; Tags::Tag::IsActive is the hook).
- Crystals offline: a daily reward gives crystals; chests give crystals too (1-2 for small chests,
  larger amounts for bigger ones) where the original had none.
- Arena, PvP and the friends system must work offline eventually (AI or local data), with the door
  left open for a future multiplayer server.
- Online features: port them, but keep them dormant (stubs behind a clear switch), not deleted.
- Platforms: macOS, Linux, Windows; maybe Android (untested by the user). Keep code portable.
- The port will be shared publicly at some point. Default kept: it reads the player's own copy of
  the original files rather than shipping Game Insight's assets.
- No content beyond 5.11 exists and no server traffic was ever captured: server-driven content is
  designed, not recovered.
- The user cannot run the original; they can describe how it behaved. Ask them when behaviour is
  ambiguous in the code.
- Work stays in milestone order (4b.4, 4b.5, 4c, ... 4h).
- Defaults kept where unanswered: original quirks/bugs stay; saves stay in the original format;
  death/revive, quick-complete and the item shop stay, paid with earned crystals; the Settings bug
  is revisited after 4c.
- The per-function coverage pass asked for before 4b.4 is done (2026-10-07); its gaps are step 4b.5.

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
   `@0x` means "ported here" to tools/inventory.py: a reference to a function that is not ported is
   written without the `@` (`AddSmoke (0x11e898)`), and an address range starts without it too.
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
       windows, GameState::secondTutorial (stand-in 0x81). (SystemFuncs settings persistence: done in
       4b.5 part 5.)
       Headless testing: `./build/rtk --screenshot f.png [--show city_rename] [--click X Y]...
       [--press X Y] [--drag X1 Y1 X2 Y2]... [--type TEXT]... [--key SCANCODE]...` runs 60 frames,
       then each input in order: a mouse input is press, 3 frames, [10 moves, one per frame,]
       release, 30 frames, in framebuffer pixels; --type/--key feed the keyboard path, then 30 frames
       (e.g. --key 28 = Y/back, 42 = Backspace, 40 = Return). `--show` is a PORT test aid that opens a
       window not reachable yet. A drag's fling uses real time, as on the original. Don't run
       rtk without --screenshot from a tool call: it opens the window and blocks.
3. [x] Game data + GameState + save/load; building and economy loops. Sub-steps:
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
       3f [x] (done 2026-10-07) farms: the farm view (Map::ShowFarm, Background::CreateFarm, farm waypoints), the
              patch entities (AIPatch), AIFarmerBig/AIFarmerSmall, Building farm functions
              (SpawnFarm, FarmCollectAndReplant, soil patch states), the farm hovers and windows.
       Note: with no save, LoadSavedGame (@0x1885e0) starts a new game on campaign map 0x15 with the
       hero entity (the tutorial), which needs M4. Until then the port boots the city from a Reset
       GameState (PORT test path).
4. [x] (closed 2026-10-18) Hero, army, quests, items, decoration jobs, the new-game tutorial. Sub-steps (planned 2026-10-07;
       each ends with a headless check, a commit and a short update to the user):
       4a [x] the hero and the army on the city map. 4a.1 (done): squads (BaseSquad/PlayerSquad/EnemySquad/
              Squad, Entity::CreateSquad +0xcc), SoldierSlots/SoldierPool, EntityManager::GetPlayer/
              GetPlayerAndSoldiers/AssignSoldiersToPlayerSquad, the hero/soldier/squad branches of
              GameState::LoadEntities/SaveEntities (g_keptEntities goes), Entity stats needed to load
              them (AP, HpMax, override attack/defense, SetupPlayerHPOverlimit). 4a.2: AIPlayer
              (movement part: Update, ClickedTile, WalkCompleted, StopMovement, UnableToWalk) and
              AIWarrior walking (soldiers follow the squad formation), the world tap through
              EntityManager::OnClick/AIBaseState::ClickedTile, Map::Load centring on the player (the
              stand-in in main.cpp goes), HUDWindow::Update's player-entity early return.
              Combat, spells, jobs and decoration actions stay stubs until 4e/4g.
       4b [x] items. 4b.1 (done): MetaExpression's parser (moved here from 4c: item bonuses are
              expressions), Tags, Items (items.xml, item_packs.xml, ItemInfo and its getters),
              PlayerItem::info on load. 4b.2 (done): GameState item APIs (AddItem, bindings/equip, belt,
              sell) and the hero's equipment bonuses (MetaExpression::OnApplyItemEffect /
              GetSummaryEffectsValue / CanApplyItemEffect, Entity stat getters). 4b.3 (done): item drops
              (ItemDrop item kind, Map::ItemPlaceContinuation), items in the shop's producing lines
              and factory boosts. 4b.4: the item windows (ItemHoverWindow, UseItemHoverWindow,
              FoundItemsWindow, NeedItemWindow, Items::GetItemOffsetInHolder). 4b.5: holes in finished
              work from the coverage pass (docs/port_inventory.md section 0).
       4c [x] quests core: MetaExpression's quest/map parts, tasks.xml (Tasks, TaskInfo, TaskSubtask),
              task state in the save (chunks already kept), progress events (the OnTaskStarted/
              OnSubTaskCompleted/OnTaskCompleted/OnTagUpdate hooks in Entity/EntityManager/Map),
              rewards. Freemium rule: propose the crystal rewards to the user before coding them.
       4d [x] (done 2026-10-11) quest UI: TaskHolderWindow (queue, quick task), TaskInfoWindow,
              TaskCompleteWindow, TaskListWindow, Shared::TaskWindow, task arrows and locators,
              the quest-locked shop items and the upgrade-lock quest names, LevelUpWindow's "new
              tasks" cell. Moved: CastleTopWindow::FeaturedQuestHolder and LevelInfoWindow -> 5;
              TaskQuickCompleteWindow and every ExecuteFirstHelp are dead code.
       4e [x] (done 2026-10-12) decoration jobs: Map::Decor Update/WorkStarted/teleports, DecorAnimController, AIPlayer's
              decoration actions (SearchDecoration, ClickedDecoration, AssignToJob, ProcessAction,
              AutoInteraction), SendGoblinToWork, Decor::UpdateOfflineState, Map::OfflineUpdate's
              decoration half, the job parts of DecorationHoverWindow/OnDecorClick.
       4f [x] (done 2026-10-13) campaign maps: Locations, spawn points (LoadSpawnPoints,
              Store/Restore), portals and the map switch (EnqueueLoadMap, ClickedPortal), fog (Fog),
              NPCs with talk tasks (TalkHoverWindow, OnTalkTo, the world dialog), AIEnemy's aggro
              zone, scripted cameras, offline goblins, ChapterWindow, the HUD's darkening and map
              resets. Moved out: the global map's windows (ChoosePath, CampaignInfo,
              CampaignComplete / Perfected, LoadingScreen) -> 5; CheckAggro, EventManager,
              PlayerHintHoverWindow, TapHoverWindow -> 4g; LoadEntitiesFromCopy -> online.
       4g [x] combat as used by the campaign: BaseCombat with AggroCombat / RangeAggroCombat,
              AIWarrior/AIEnemy fight states, damage (Hit, Block, Death, drops), arrows,
              BattleBarWindow, BeltBarWindow (potions, orbs), HealSquadWindow, the hero's speech
              bubble, traps, HP regeneration and death (user decision: free regeneration, crystals
              to heal at once). Moved out: the siege system (dead in 5.11); bosses, arenas, PvP,
              spells, the rage bar, soldier healing -> 5.
       4h [x] (closed 2026-10-18; the tutorial plays to step 0x61, its end moved to 5a-5d) the new game: LoadSavedGame's new-game branch (hero 0xc + squad + soldier 0x1a2 on map
              0x15, location 2; replaces the PORT test city), TutorialWindow::Update steps 0..0x17
              (a jump table Ghidra lost: read from the asm), BuildingHovers::ShowWorldDialog,
              SelectHQWindow, TutorialSender (no server: a no-op), then the city tutorial
              (UpdateCity, UpdateSecond, the tutorial arrows left as UNVERIFIED in 3e/3f).
       Not in M4 (later milestones; ask before porting the online/freemium ones): spells and magic,
       arena/PvP/boss/siege/async combats, chests, crafting and professions, the item shop,
       collections, the global map, presents (chunk 8, friends' gifts), CharacterInfoWindow.
5. [ ] The rest of the game (planned 2026-10-18). 1290 functions at the start. BITE-SIZED STEPS (user
       2026-10-18): each step is one sitting of about 20-40 minutes, ends with a build, a short
       check in the test loop, a commit and a 2-3 line update to the user, then WAIT for the user
       before the next step. A step that grows past that is split at a natural seam and the user
       told. "ASK" = a freemium/online question for the user before the step starts.
       The army and the tutorial's end:
       5a [x] (2026-10-18) HireTroopsWindow 1: windows/HireTroopsWindow.cpp: Init, the LoadFrom structs
              (WarriorInfo 0x3e4 bytes x4 cards, CostInfo, ButtonWarriorBuyNow, UpgradeInProgress;
              Shared::WarriorSlotInfo @0x345ab0), Show / ShowForBuilding / Hide / SetZ / ZRange / Click
              (the soldier drag between squad and reserve) / OnTab / OnLeft / OnRight / Move / OnBack /
              OnTweenOut / SoldierMove, GameState::AddSoldiersToSquad, game/Tavern.h (TavernInfo's
              fields read so far). Wired: BuildingHovers kHireTroops, OnBuildingClick (0x8f and class
              0xc with "always_open_tavern"), HoverWindows cls 0xc, TaskInfoWindow step 0x61. Left as
              notes: UpdateContents (5b, empty), the buttons' actions, Update, OnTweenIn (5c; stubs
              without @ tags), HealHoverWindow (5e), the other ShowForBuilding callers (BattleBar's
              Army, TasksHelp, TaskListWindow, ItemHoverWindow, FoundItemsWindow, PlayerWeakWindow
              removed). VERIFIED (test loop, tap 2700:690:265 after the CITY run): at step 0x61 the
              barracks opens it in the short tutorial layout (6 tabs, 3 cards, arrows, no squad /
              reserve), no GUI errors; X does nothing there (the original locks input to card 1's
              Train button). NOT checked: the full layout outside the tutorial.
       5b [ ] HireTroopsWindow 2: UpdateContents 0x2d05e4..0x2d7ae8 (30 KB). SPLIT (2026-10-18):
              5b-0 [x] the data it reads: game/Tavern.* (Init "tavern.xml" in main_Loop_Init 22/46,
                   InitSpecial: the goblins from "goblin_cost", EnumTavernInfo / GetTavernInfo /
                   GetForEntity / GenerateHP; TavernInfo 0x30 bytes, its "type" is the window's tab),
                   EntityData::GetCostGold/Crystals take an offer's own price, GameState::Get/Set
                   TavernWarriorTS/HP (g_warriorRespawn, chunk 0x4d). TOOLS: Ghidra decompiles
                   UpdateContents with raised limits over its range (DecompRange.java: out/big/uc.c,
                   4280 lines once resolved); DecompRangeAddr.java prints every C line with its asm
                   address (out/big/uca.c; drop the "--------" lines: out/big/ucb.c); fnres.py takes
                   that file as its 2nd argument. The C has gaps (empty if/else where stores were
                   dropped, e.g. the card's HP text: "?" when hpMax is 0, else the number, asm
                   0x2d3dcc / 0x2d5778): check every branch in the asm (out/asm_all.txt).
                   Layout of the C: lines 4-208 header (the reserve list 0x622904, tab modes and icons
                   army_tavern_40 .. goblin_60, the tab's offers g_offers sorted by slot), 209-3130 the
                   four cards (one loop over local_1e10 0..3, entry firstIndex + n), 3130-3860 the squad
                   (HIRE_TROOPS_SQUAD) and reserve (HIRE_TROOPS_RESERVE, SLOTS_TAKEN, IN_QUEUE,
                   WARRIOR_TRAINING_SMALL, HIRE_TROOPS_UNLOCK) slots.
              5b-1 [x] (2026-10-18) the header (reserve list, tab modes / icons, the tab's offers
                   sorted with StlSort, the building's name / level / upgrading) and the cards (C
                   lines up to 0x2d1860: respawn TS / HP on first sight, levelLock / trainable /
                   canTrain / buyCrystal / goblinOwned / tamerTask, HP and attack texts ("??" when 0),
                   attack icon, name, the cost lines, the slot window, Train / Buy now / the two small
                   buttons, crystal prices), the centring of fewer than 3 cards, the scroller's
                   count; SortTavernInfoBySlot, GetAdjustedResourceCost, ButtonWarriorBuyNow::
                   ShowPrice / ShowPriceDiscounted. VERIFIED at step 0x61 (barracks tab): Soldier
                   8 health / 1 damage, costs 1 item, 25 gold, 25 food with check marks; Fighter
                   locked. button_build_upgrade and button_tavern_hang still show their layout state
                   over the Train button (5b-2). Debugging tip: dump a card's visible windows
                   recursively (name, x, y, w, h) to tell overlapping buttons apart.
              5b-2 [x] (2026-10-18) the rest of each card (0x2d1878..0x2d2b38): requirement line
                   visibility, the refresh button (TAVERN_REFRESH, GetResetCost @0x2ce2e0, enabled
                   after tutorial 0x7f) and the timer (TAVERN_AVAILABLE_IN / the tamer's "%d/%d"),
                   the requirement text (building / level / task locks, SPAWN_GOB lines, STASK names
                   for quick tasks), "%s_DESC" / PROMO_TAMER, the hired goblin's line, the build
                   button (task: TASK_BUTTON_EXEC / PERFORM_FIND; next level: PERFORM_UPGRADE;
                   further: PERFORM_PREVIOUS_UPGRADE) with g_buildTask[n] (0x622928), the sale label.
                   VERIFIED at step 0x61: Soldier's Train clear; Fighter "Complete quest 'Battle
                   Preparations'" + Complete (disabled); Berserk's description + Hire 25 crystals.
                   Note: asm_all.txt's string comments are wrong for some literal pools near page
                   ends; check with elfread (u32(pool) + pc + 8, cstr) or trust the decompile.
              5b-3 [x] (2026-10-18) the squad and reserve slots (0x2d2b70..end): squad soldiers with
                   clamped HP bars, empty / locked helmets; reserve soldiers (HP bar, or the
                   5-second training bar from the building's stateTime with WARRIOR_TRAINING_SMALL),
                   queued ones (HIRE_TROOPS_IN_QUEUE, faded), Empty, the unlock slot (gold_add,
                   HIRE_TROOPS_UNLOCK; not at tutorial 0x61/0x62), Locked; SLOTS_TAKEN n/m.
                   VERIFIED with the full layout forced at step 0x61 (temporary edit, reverted):
                   Squad 2 open + 1 locked, Reserve 2 Empty + 3 Locked, Slots Taken 0/2.
       5c [x] HireTroopsWindow 3. SPLIT (2026-10-18, about 27 functions / 3,500 asm lines):
              5c-1 [x] (2026-10-18) OnTrain 0x2d9a1c (goblins' level check, no-slots popup, the tamer's
                   "find a beast" arrow / map switch / SERVER_ERROR popup, NotEnough with
                   REQUIREMENT_TROOPS + PERFORM_HIRE, charges gold / resource / item; goblin -> HireGolbin;
                   tavern 0x8f -> new Entity + HireSoldier(e, true, 1); city map -> SpawnEntityAt the HQ's
                   spawn tile + Building::LaunchContract(Entity*) @0x127294 (contract 0xbad1dea, worker 0,
                   CenterOn, SoldierPool); else CreateEntity + HireSoldier(e, true, 2); tutorial 0x61 ->
                   0x62 + interaction lock), OnHire 0x2d8b90 (crystals, ConfirmPurchaseWindow),
                   RunHireAnimation, Update (hover z, tutorial 0x63 -> 0x64, the glides: ease-out circle
                   0.5 s, reserve fades in; refresh each second), OnTweenIn (arrow at Train / small Hire /
                   build), ShowSlot, ShowEntity. TavernInfo +0x1c/+0x20/+0x2c renamed attack / defense /
                   level. VERIFIED (case 3 + tap 2820:410:552): the arrow points at Train; Train charges
                   25 gold + 25 food, step 0x61 -> 0x62, the window closes, the soldier (8 HP) spawns at
                   the castle, walks to the barracks and shows in the Army bar. NOT tested: OnHire (input
                   is locked to Train at 0x61), the tamer path, other maps, the glides (level < 5 hides
                   the window instead). Left for 5d: Building::TrainingCompleted 0x11d950 /
                   IsTrainingCompleted / Map::TrainingcontractRunning (the trained soldier walking out,
                   step 0x62 -> 0x63; OnContractCompleted's class 0xc note).
                   Note: "TutorialWindow::UpdateCity() Amount of lumber after tutorial (311) ..." is
                   printed at f=780 of case 2/3 already (before this step).
              5c-2 [x] (2026-10-18) OnItem (cost cell hints: item / gold / resource), OnRefresh (new
                   offer for crystals: the tab's respawn counts as passed), OnSquad (soldier hint with
                   Move / Dismiss, locked slot: WAR_SLOT_LOCKED + HIRE_TROOPS_TASK_LOCK or
                   SHOP_UNLOCK_TASK), OnReserve (soldier hint unless working; the first closed slot ->
                   the hurry bubble HIRE_TROOPS_RESERVE_EXPAND / UNLOCK with reserve_slot_cost), OnHintGold
                   (buys the reserve slot), OnHintMove, OnHintDismiss (+Continue: removes the soldier);
                   ItemHoverWindow::OnTrain / OnMove / OnDismiss now call the window. g_unk60f2ac is
                   g_tappedUnit (only written). port_plan: FUN_002d00d4/0174/0260 util (std::sort with
                   SortTavernInfoBySlot), OnMove dead (no caller). OnBlueButton moved to 5c-3 (it routes
                   to OnBuild / OnUpgrade / OnTaskDo). VERIFIED (case 3 with a temporary full layout and
                   no input lock, reverted): the Cookie cell hint (Food / Bakery), the locked squad slot
                   ("Locked / Complete Quest": squad_slot_0x_quest_unlock is a server setting, 0 offline,
                   as in BattleBarWindow), the reserve unlock bubble (Unlock 5 crystals), Unlock -> 0
                   crystals and Slots Taken 0/3. NOT tested: Move / Dismiss (no soldier in reserve yet),
                   OnRefresh.
              5c-3 [x] (2026-10-18) OnBlueButton (no training building -> OnBuild; a task in
                   g_buildTask -> OnTaskDo (TaskInfoWindow), the Fighter 0x195 instead -> the world map
                   arrow at map 0x52, or on it AddCameraPoint(0x10, 0xe ...); else a too-low level ->
                   OnUpgrade), OnBuild (the unfinished site: camera + arrow with OnBuildingClick, else the
                   shop on it; elsewhere the world map arrow at the city), OnUpgrade (PEOPLE_LIMIT popup,
                   else CheckUpgradeRequirements + UPGRADE_ACTION -> OnUpgradeContinuation), OnTaskDo,
                   ExecuteFirstHelp (the bot: "bot_use_cb"). HUDWindow::GameReload is dead (no call,
                   pointer or relocation in 5.11; port_plan). UNVERIFIED (milestone 5s): OnBlueButton
                   hides ArenaDifficultyWindow / PlayerCompetitionWindow first. VERIFIED (case 3, temp:
                   full layout, input unlocked, the right arrow calling OnBlueButton(1); reverted): the
                   Fighter's button -> the window closes, the world map opens with the arrow at map 0x52
                   ("!" 0%). NOT tested: OnBuild / OnUpgrade / OnTaskDo (no card offers them this early:
                   the archery tab's cards are quest-locked without a build button).
       5d [x] the tutorial's end: play steps 0x61 -> 0x80 (warrior trained and walking out, the envoy,
              the compass, the world map); a save round trip on a copy. The opening tutorial is done.
              SPLIT (2026-10-18): 5d-1 [x] Building::TrainingCompleted 0x11d950 (contract cleared; the
                   trainee joins the squad and walks to a waypoint near the hero when there is room and
                   none of its kind, else the reserve hidden; silent: the squad at once; step 0x62 ->
                   0x63 and the barracks' order 0 gets its real training time; the train subtasks),
                   wired in OnContractCompleted (class 0xc). Dead (no caller in 5.11):
                   IsTrainingCompleted, Map::TrainingcontractRunning. VERIFIED (test loop: case 3 +
                   2820:410:552 Train, 3500:640:646 Ok, 3900:1240:38 compass; SL every 50 frames to
                   3300 and 3550..4050): the soldier trains 3 s, 0x62 -> 0x65, "Quest Complete! Warrior
                   for Hire" (210 XP, 80 gold, 50 wood, 50 stone, paid on Ok), the soldier walks to the
                   hero, the envoy ("Skeletons... follow me west!") with the arrow at the compass, the
                   compass opens the world map at step 0x67 with the arrow on the next node.
                   BLOCKED: that node is campaign 1 (not the instant-travel node 4000), so its tap needs
                   CampaignInfoWindow (5f). 5d-2 [ ] after 5f: the node -> map 0xe -> step 0x80
                   (Map::Load), then a save round trip on a copy (the game does not save in the tutorial).
                   PROGRESS (5f-1, 2026-10-18): the node -> CampaignInfoWindow -> Begin Campaign ->
                   map 0xe -> step 0x80 / secondTutorial 0x200, Chapter 4 "The Early Feats" (test
                   case 4 in tools/test_inputs.txt).
                   5d-2 [x] (2026-10-18) VERIFIED, saves on copies: (1) the map switch itself saves
                   (the city at step 0x67, before map 0xe loads); reloaded, the city comes back at
                   0x67 with the arrow on the compass, the soldier and the same resources. (2) On
                   map 0xe after the Chapter 4 banner (step 0x80 / secondTutorial 0x201) a --save
                   writes map 14 too; reloaded, the game starts in the city (LoadSavedGame always
                   loads map 0, as the original) with 0x80 / 0x201, the crystals and the soldier
                   kept. Nothing needed changing.
                   The world map's "shift" (paper on the left and bottom at 1280x800) is FAITHFUL:
                   home and the tutorial's node sit in the map's bottom-left corner, CenterOnMap
                   centres on them and the scroll clamp (HandleMove / SetZ, checked against the asm)
                   lets the map pass its edges by half a margin (256 x the window scale, about 170 px
                   here), drawn with pattern_paper_bg; the bottom strip is wider because
                   world_map_bg has paper below its frame. (CenterOnArrows' x term is not negated,
                   unlike y: also the original's, asm 0x2c23c8.)
       5e [x] healing and repair: HealHoverWindow, BattleBarWindow::ShowHealSoldier, HireTroopsWindow::
              ShowHealSoldier, RepairWindow. SPLIT (2026-10-18, about 31 functions):
              5e-1 [x] (2026-10-18) hud/HealHoverWindow.* (Hint_heal_warriors.xml): "<PERFORM_SAVE>
                   <unit> hp/max", the potions of "heal_warrior_items" (0x162 only when owned) in a
                   scrolled 2x5 grid (cells numbered by column, 0x5a678c), quality frames, "x%d"
                   owned, "+%d stats_hp_points" from the item's HP effect on the green badge's text;
                   OnItem: not owned -> NeedItemWindow (HireTroopsWindow hidden); else the effect is
                   applied, the potion removed, a dead soldier stands up (units' layer, back into the
                   hero's squad when it has a slot, spawned near the hero, AI reset, camera on it),
                   the windows refreshed; the card stays while the unit is still hurt. A tap off it
                   closes it. BattleBarWindow::ShowHealSoldier (over the army slot) and
                   HireTroopsWindow::ShowHealSoldier (over the squad / reserve slot), their heal cards
                   (0x617a30, 0x622220) take taps first; ItemHoverWindow::OnHeal calls them.
                   UNVERIFIED: ArenaDifficultyWindow / PlayerInfoWindow / PlayerCompetitionWindow hidden
                   on the NeedItem path (5s / online), the "drink_potion" sound (6).
                   VERIFIED (the 5d-2 save; new test aid RTK_HEALCARD=1 in tools/test_hook.txt opens the
                   card over the first hurt soldier; RTK_SHP=3 RTK_ITEM=56): "Heal Soldier 3/8", the
                   grid with +1/+3/+5/+4/+2 health badges, x3 on potion 56; tapping it heals to 4/8, x2,
                   the card stays; an unowned potion -> NeedItemWindow, which leads to the Bakery.
                   The army card's Heal button only shows for elite soldiers (as ported in 4g), so
                   the tutorial soldier's card has none.
              5e-2 [x] (2026-10-18) windows/RepairWindow.cpp (Window_repair.xml): the item (name in
                   its quality's colour, frame / broken, picture), the durability bars before and
                   after (dark red base, red the current maximum, green what is left; "%s/%s" of
                   REPAIR_DURABILITY), three methods: Quick Repair (0.5 + 20% of the missing points)
                   and Quick Mend (0.5 + 50%) for gold, both capped at the current maximum, which
                   becomes the new maximum; Professional Repair (all of it) for crystals.
                   GetRepairCostGold / GetRepairCostCrystals: half the item's price per its full
                   durability (the other currency through exchange_gold, else the repair_* settings),
                   at least 1 (crystals with crystal_mult). OnRepair: NotEnoughWindow when short,
                   task 0x597's subtask 0x3c. A method with nothing to do is disabled; an item at its
                   maximum preselects crystals; Hide resets to Quick Mend. Wired: ItemHoverWindow::
                   OnRepair. Not wired: CharacterInfoWindow's own opening (5l). UNVERIFIED: the
                   sounds (6), ItemShopWindow::UpdateContents (5q); the purchase statistics are online.
                   VERIFIED (the 5d-2 save; new test aid RTK_REPAIR=1 gives a worn Antique Sword
                   10/26/30, cost 2000 gold, and opens the window): Quick Repair 134, Quick Mend 334
                   (selected, after 20/30), Professional 5 crystals; Repair with Mend: 1144 -> 810
                   gold, closed; Professional + Repair: 6 -> 1 crystals, closed.
       The world map's windows:
       5f [x] ChoosePathWindow, CampaignInfoWindow, HintHoverWindow (campaign starts from the map).
              SPLIT (2026-10-18, about 73 functions; ChoosePathWindow::UpdateContents alone 6.7 KB):
              5f-1 [x] (2026-10-18) windows/CampaignInfoWindow.cpp (Window_campaign_brief.xml):
                   SetParameters (the map's finds: resources 0..10, items not packs), UpdateContents
                   (range-decompiled 0x2731a8..0x2742f4 with DecompRangeAddr: out/big/ci_uc*.c;
                   header, the two-part progress bar (main fills the orange half from the left, side
                   the green half from the right), the % counter, the newest running main / side
                   quest's icon else gold_confirm / gold_quest, the findable items from the
                   location's "items" in a scrolled 2-per-line grid with the found counts, one or two
                   centred, the "complete" tick (its tap shows the grid), mentor / sultan (campaign
                   5), Begin / Resume), ShowItem, OnItem (ItemHoverWindow), OnDialogShow / Hide,
                   OnTweenIn (arrows: ShowItem's cell; the mentor before map 0xf was entered),
                   OnStart (SwitchMapAnimated after SetAnimationOnce, else SwitchMap), Click, Back,
                   Show / Hide; HintHoverWindow (hud/HoverWindows: Dialog_hint_hd.xml a third
                   wider, centred, scale 1.3). Wired: CoreLocation::OnClick and the desert castle
                   (CastleLocation 0x3ea). UNVERIFIED (milestone 5n): the profession filter of the
                   findable items (every item is listed). Not wired yet (their windows are later
                   steps): TasksHelp's ShowItem path (with RaidInfoWindow), NotEnoughWindow's and
                   ItemHoverWindow's hide lists. VERIFIED (test case 4): the node opens "Campaign -
                   Digging Up the Past", 0%, two items, no X at 0x67; the mentor shows the story
                   bubble with Next; Begin Campaign travels to map 0xe (step 0x80).
              5f-2 [x] (2026-10-18) windows/ChoosePathWindow.cpp (Window_where_to_next.xml): a
                   strip of 3 recycled buttons over 6 slots (home, campaigns, missions, winter,
                   desert, shop; the slot is OnTravel's type, the background type comes from
                   g_options), names / progress lines ("%s: %d%%" of GetCampaignProgress) / the
                   running mission's name and time (missions take turns every 5 s, faded), locks,
                   the dots, the middle slot's description; PvP and arena featured holders (6, 7);
                   OnTravel (arrows on the world map, quest helps via HelpWithTask / TaskInfoWindow,
                   the desert's sultan steps, the arena's travel), OnTravelRegular (a cut-off button
                   scrolls in), Show (starts at the newest open part), Update (the second half was
                   Ghidra's bogus _INIT_78: range-decompiled into out/big/cp_up*.c; UpdateContents
                   into out/big/cp_uc*.c). Wired: GlobalMapWindow's Navigate, Entity::OnTalkTo 0x1d7,
                   CastleTopWindow::OnReturn (task 0x43f done), TaskHolderWindow 0x5fb,
                   Map::Load's RunShowOnLoad. CenterOn is dead (port_plan). UNVERIFIED: the PvP rank
                   and the arena events' reputation texts (Professions, 5n), ItemShopWindow (5q),
                   LevelInfoWindow's PvP / arena helps (5n), SultanArrivingWindow (5h),
                   ArenaEventWindow (5s); CampaignCompleteWindow's and RaidCompleteWindow's openings
                   come with 5h / 5g. VERIFIED (the 5d-2 save, city at 0x80: compass 1240:38,
                   Navigate 680:745): the window with PvP "Valor - Beginner (0)" and Arena disabled,
                   Kingdom in the middle ("Return to Kingdom"), 6 dots; the right arrow twice ->
                   "Easy money" locked with its description; tapping Kingdom closes it and puts the
                   arrow on home.
       5g [x] raids: RaidInfoWindow, StartRaidingWindow, RaidCompleteWindow. SPLIT (2026-10-18, about
              60 functions; RaidInfoWindow::UpdateContents alone 13 KB):
              5g-1 [x] (2026-10-18) windows/RaidInfoWindow.cpp (Window_raid_brief.xml; UpdateContents
                   range-decompiled into out/big/ri_uc*.c, Ghidra's bogus _INIT_112 cuts it): the
                   header, the star tabs (epic at 3), the star bar with "Raid Complete!" and the
                   overall %, three flags (gold x raid_star_gold_mult, XP, the items sorted by
                   GetItemDropProbability: location table, else the enemies' items_drop / starN_items,
                   > 10 high, > 4 medium) with the found counts on the current star's flag, the
                   "complete" tick, greyed flags above the next star, the completion animation
                   (RunCompletionAnimation: the bar fills over 0.5 s, the new flag fades in; triggered
                   by RaidCompleteWindow, 5g-2), the timer states (not started / running / time up /
                   timeout / after the run), the three start buttons (begin, resume, restart, crystal
                   resume with OnGold and ConfirmPurchaseWindow), the requirements; OnBlue (pays the
                   star's requirements via NotEnoughWindow, restarts quests, resets an expired map:
                   ResetTask / StartTask, removeItems, ResetMap, boss fights, collection, times),
                   OnClose, OnItem (ItemHoverWindow with the chance), ShowItem (the flag order table
                   0x59fbf0), Update (an expired mission map opens it by itself). New:
                   Shared::ButtonRaidStart, StringTable::GetExtendedTimeString. Wired: the world map's
                   mission nodes, its Go-home button in a timeout, HUDWindow::OnFarmTimer, the quest
                   help's item search on a mission / campaign map (Tasks::HelpWithMapSwitchTo),
                   ItemHoverWindow's check. UNVERIFIED: professions (5n: item filter, profession
                   probabilities, REPUTATION requirements), RaidCompleteWindow::ResetRaid (5g-2),
                   sounds (6); purchase statistics are online. Not wired: CraftItemWindow /
                   CraftingWindow OnFind (5o), NotEnoughWindow's and ItemHoverWindow::OnFind's notes.
                   VERIFIED (5d-2 save; new test aid RTK_RAID=n opens the n-th mission, RTK_RAIDDONE=1
                   as after its end): "Raid - Troll Trail", Royal / Heroic locked, 0%, the flags
                   (gold 161 / 241 / 322, XP 208, the item chances), "Time limit: 1 hour", "Cost to
                   begin: 50"; Begin pays 50 gold and loads map 87; after the run: no X, 33%.
              5g-2 [x] (2026-10-18) windows/RaidCompleteWindow.cpp (Endscreen_raid_complete.xml): the
                   banner slides down over a 37% black band (alpha 0.6) with the gold line, the
                   squad cheers, the camera glides to the hero, the loot strip rises
                   (FoundItemsWindow::SetParameters(LocationInfo*): every map's finds, the first
                   completion's "farm_complete_cb" crystals first; FoundItemsWindow::Click is now
                   public); its Ok slides the banner up and opens RaidInfoWindow as after the run
                   with RunCompletionAnimation (not at 3 stars); the brief's button runs ResetRaid
                   (quests reset and hidden, remove_items, the map reset and loaded again, the first
                   completion adds min(farm_complete_cb, 5) crystals, a new star completes subtask
                   0x29 / star + map * 10 + 1, end time; then the world map, SafeSave, ChoosePath).
                   ForceHide (silent reset) on exit. windows/StartRaidingWindow.cpp
                   (Window_start_raiding.xml, "Easy Money"): hero / heroine picture, Start Raid!
                   opens the world map with an arrow on every open mission. Wired: GlobalMapWindow's
                   CoreLocation::ShowCompletionWindow (missions), RaidInfoWindow::OnBlue,
                   FoundItemsWindow::OnClose, HUDWindow's darkening, the back key's exit question,
                   main_Exit_Func, CastleTopWindow::OnReturn's offer (easy_money_delta / _threshold;
                   GetGoldEarned is always 0 in 5.11, so it never shows). PORT (online): the star
                   request OG::MakeRequest, QueueSendSave. UNVERIFIED: the sound (6); the ForceHide
                   calls in ReloadSavedGame, GameState::CleanBeforeLoading and HUDWindow::GameReload
                   (those functions are not ported yet). VERIFIED (the 5d-2 save; new test aids
                   RTK_RAIDEND=n opens the n-th mission's RaidCompleteWindow, RTK_EASY=1 the offer):
                   "Raid Complete!", the band darkens, "During this raid you found: 2 crystals";
                   Ok (655:770) -> "Raid - Troll Trail" brief, no X, the bar animating to 33%;
                   Begin! (625:640) -> map 87 loaded and saved, "Adding 2 crystals ...", 6 -> 8,
                   the world map with "Select a path!". Easy Money: Start Raid! (630:610) -> the
                   world map; at level 30 two "Start" arrows on the open missions; X (805:188)
                   closes it. Not played: a real mission's end (its end tasks) and quitting while
                   the banner is up.
       5h [x] CampaignCompleteWindow, CampaignPerfectedWindow, LoadingScreenWindow, SultanArrivingWindow.
              SPLIT (2026-10-18, about 56 functions):
              5h-1 [x] (2026-10-18) windows/CampaignCompleteWindow.cpp (Endscreen_campaign_end.xml)
                   and windows/CampaignPerfectedWindow.cpp (Endscreen_campaign_100_percent.xml),
                   RaidCompleteWindow's siblings: banner over the 37% black band, cheering squad,
                   loot strip. Complete: "CAMPAIGN_YOU_FOUND" (the campaign's finds after the
                   "campaign_complete_cb" crystals), "<name> completed" in text_01 / text_02; Hide
                   leaves the queue at once, location 0xb enqueues ChoosePathWindow's show-on-load;
                   the slide-out shows the world map and SwitchMapAnimated(0) home (Show's code
                   0x272460..0x272770, Ghidra's bogus _INIT_70 cuts it). Perfected: SetParameters
                   (location, 100% task) sets the task's no_complete / do_not_show_reward; the strip
                   is the task's reward (TASK_COMPLETE_DESC, "campaign_complete_full_cb" crystals
                   first), "CAMPAIGN_PERFECTED" / "CAMPAIGN_PROGRESS" / "100%"; Hide blinks the
                   compass (CastleTopWindow::BlinkCompass now ported); the slide-out completes the
                   task with its reward dropped; ForceHide on exit. Wired: Location / CoreLocation::
                   ShowCompletionWindow, OnTaskCompleted's 100% branch, FoundItemsWindow::OnClose,
                   HUD darkening, the back key, main_Exit_Func. PORT (online): QueueSendSave.
                   UNVERIFIED: the sound (6); ForceHide in the reload paths (not ported yet).
                   VERIFIED (the 5d-2 save; new test aids RTK_CAMPEND=n / RTK_CAMPPERF=n open the
                   n-th campaign's window, RTK_GOMAP=m switches to map m at frame 30, RTK_AT=f
                   opens the windows at frame f): "Campaign Complete!", "During this campaign you
                   found: 1 crystal"; from map 14 (RTK_GOMAP=14 RTK_AT=200) Ok -> world map, travel
                   home, "Select a path!" after the city loads (from the city itself the same-map
                   switch only hides the world map: faithful). "Ultimate Completion!", "Progress:
                   100%", the reward strip; Ok -> "Done task 1056", crystals 6 -> 12, LevelUpWindow.
              5h-2 [x] (2026-10-18) windows/LoadingScreenWindow.cpp (Visit_friend_loading_screen.xml:
                   small logo moved to layer 0xf, the loading cogs, "GOTOBATTLEMAP") and Map::Load's
                   loading frames: the flag 0x60f014 (cleared when the world map is not visible at
                   the start; IsVisible is false during the sepia, so animated switches never show
                   it, as on the original), two frames first, then Show + minLayer 0xe +
                   clearColor off + a frame, a Render::Update between the stages (10 on the
                   original; the port's Map::Load is laid out differently, so they sit at the
                   nearest matching points), at the end minLayer 0, a frame and Hide, the flag
                   set again. windows/SultanArrivingWindow.cpp (Window_ariving.xml, "New
                   Campaign"): the time left of "sultan_move_time" from event 0x5dd on the clipped
                   progress bar ("SULTAN_ARRIVES_IN"), refreshed every second; "Arrive now" for
                   "sultan_unlock" crystals (NotEnoughWindow when short) completes 0x5c9, centres
                   on 0x53, the sultan's movement and SwitchMapAnimated to 0xb, then RunLastHelp
                   (Map::Load's end) runs quest 0x591's help; "Collect items" opens TaskInfoWindow
                   for 0x5c9. Wired: the world map's sultan marker (OnSultan 0x2bf434), the desert
                   node, ChoosePathWindow's desert slot, CastleTopWindow::OnReturn, Map::Load.
                   PORT (online): the Expansions checks, Billing::LogCBPurchase. UNVERIFIED: the
                   sounds (6). VERIFIED (the 5d-2 save; test aids RTK_SULTAN=1 opens it,
                   RTK_SULTANT=s starts event 0x5dd s seconds ago, RTK_WORLD=1 opens the world map
                   at frame 30): "Sultan arrives in: 7 days 00:00:00" (not started), "3 days
                   12:00:00" with the bar half orange and the text split at its edge; Arrive now
                   (520:560) with 100 crystals -> "Done task 1481", 50 crystals, the trip to the
                   Imperial Palace (map 11), the elephant, the help arrow on quest 0x591. The
                   loading card ("Traveling the Empire...", the logo, the cogs) seen with a
                   temporary edit that kept it after RTK_WORLD=1 RTK_GOMAP=14; the animated switch
                   above skips it (faithful).
       Fights:
       5i [x] bosses: BossCombat, BossCombatWaves, BossOutroWindow, BossTimeHoverWindow, BossIntroEffect,
              FastBossIntroEffect, BossIntroWindow::SetReputation.
              SPLIT (2026-10-18, about 52 functions; BossCombat alone ~16 KB):
              5i-1 [x] (2026-10-18) BaseCombat::SwitchToBossFight (the boss map's load, mReturnMapID,
                   mBossSpawnID, mLastBossXY / PlayerXY, mBossMapPositions defaults and "bat" list,
                   BossCombatData::mLastBossMeta copy, mBossWaves from the BOSS waves with 0 between
                   waves -> type 4, the boss last); game/BossCombat.cpp (vtable 0x607ea8): BeginCombat
                   (places 0 / 2, RestoreBossCombat, run to 1 / 3), RunningOut (state 0x10),
                   WaitingForBossBattleClick (the 0x6c tutorial step), Update (ArenaTurnWindow: turn,
                   whose turn, arena_turn_time countdown, then it goes on by itself; BottomBattleWindow
                   buttons), Continue (enemyFirst), UpdateAttackPosition (the fighting area 8..11),
                   StartNewTurn (range-decompiled out/big/bc_snt.c; enemyFirst: initiative 0 / 100),
                   EndTurn (HP into GameState::mBossFights), RestoreBossCombat (record / new, restore_time,
                   SpawnAt both squads, the copied meta), OnEnemyDied, EndCombat. New: GameState
                   BossFight / timers accessors, GetBossFightStartTime; Entity::GetBossWaveInfo /
                   AdditionalWavesInfo / TaskID / MapID; TaskInfoWindow::SkipBossTask, OnFight (the boss
                   form's Fight). Wired: Map::SpawnEntities (type 3), AIPlayer::ClickedEntity and
                   AIEnemy::CheckAggro (bosses are attackable again; a boss quest opens the boss form).
                   SwitchToArenaFight moved to 5s. UNVERIFIED: BossOutroWindow (Show, IsVisible,
                   SetBossName: 5i-2) so a won fight does not return yet; BossCombatWaves (type 4) and
                   BossIntroEffect (5i-3); the arena-1 statistics on map 0x70 (5s); music / sounds (6).
                   VERIFIED (the user's ~/rtk-test save copied; new test aids RTK_BOSS=1 taps the
                   nearest boss at RTK_AT, the hook prints ctype / cstate / paused; SL sleeps are
                   needed: test frames run without waiting, so 0.4 s of game time takes thousands of
                   frames): map 15's skeleton boss 216 (BOSS=473:2, MAP_ID 108): the AggroCombat's
                   shield pause -> SwitchToBossFight -> map 108 -> BossCombat; the two 473 skeletons
                   and the boss take their places, "Turn 1 / Your turn", the countdown, the hero's
                   turn when it runs out, a skeleton killed, "Turn 2" with 8 s left.
              5i-2 [x] (2026-10-18) BossOutroWindow (windows/BossOutroWindow.cpp: "Victory! You have defeated"
                   banner over the black band, the squad cheers, FoundItemsWindow's loot strip; OnTweenOut
                   loads mReturnMapID again; OnBack; queue between BossIntro and BottomBattle), BossTimeHoverWindow
                   (hud/HoverWindows, hover type 0x10, wired in UpdateHovers / SetHoverType), Map::DropBossReward,
                   Map::UpdateSpawns' boss part (a saved fight with 0 HP = dead boss + reward + subtasks; a won
                   combat ends: boss dead at mLastBossXY, hero back, victory pose, reward) and the
                   "pending arena reward" drop. Hooked into FoundItemsWindow::OnClose, HUD darkening,
                   BossCombat / ArenaCombat Update, BossCombat::EndCombat / RestoreBossCombat.
                   MOVED to 5n: BossIntroWindow::SetReputation (needs Professions::GetProfession).
                   UNVERIFIED: the arena parts of Show / Update (profession bar, 5n) and the arena sound (6);
                   PORT: OG / OG2 requests (online). Not ported: UpdateSpawns' PvP shadow part (4h/5s).
                   VERIFIED (same save, RTK_GOMAP=15 RTK_AT=150 RTK_BOSS=1, new test aid RTK_WIN=250 sets the
                   enemies to 1 HP, SL sleeps every 15 frames from 160, about 3000 frames): the outro
                   appears with the dropped loot strip; tapping Ok (640:770) at frame 3000 slides it away,
                   map 15 loads, combat ends, the boss lies dead, its armor drops with the helper arrow.
              5i-3 [x] (2026-10-18) BossCombatWaves (game/BossCombat.cpp, type 4: BeginCombat, WaitingForContinue
                   leads the next wave in when the one in play is dead, OnEnemyDied, RestoreBossCombat sorts
                   mBossWaves' groups into waves; wired in Map::SpawnEntities), BossIntroEffect +
                   FastBossIntroEffect (game/BossIntroEffect.cpp: the 3.85 s zoom / blur / white transition
                   around a boss map's load, wired in Map::Update; FastBossIntroEffect is empty in 5.11),
                   Render::BlurTexture (PORT: on the CPU, same 5-tap sums), shader 6 (boss_intro) and
                   Render::MarkDirty. PORT: the capture sprite has no flip (CreateScreenTexture flips rows).
                   VERIFIED: the effect runs (120 frames, no shader errors; its look was NOT viewed: only the
                   last frame is screenshotted); a forced two-wave fight (a temporary edit of
                   SwitchToBossFight: waves {473, 0, 473, boss}, taken out again) goes type 4, wave 1,
                   the next wave runs in (state 0x10), the boss, then BossOutroWindow.
                   5i is DONE.
       5j [ ] spells 1, split in 5j-1 .. 5j-3 (2026-10-18: too big for one sitting):
              5j-1 [x] (2026-10-18) Spells (game/Spells.cpp: spells.xml, 9 spells, GetBestSpellForSlot
                   / GetSpellLockedByTask; Init wired in main between items and locations, its two
                   load-screen step numbers 0xc UNVERIFIED), Entity's magic (EntityCombat.cpp:
                   WillDieMagic, OnHitMagic with the map's SPELL_RESIST (0x79) per slot, OnSpellCast x2
                   = the view goes to the epicenter, SetDefenseMagic, SetEffectOwner, OnBlockMagic,
                   OnAttackMagic / OnDeathMagic empty). VERIFIED: builds, the game loads 9 spells; no
                   caller yet (the AI side is 5j-2), so the damage code is untested.
              5j-2 [x] (2026-10-18) AISpell (AIState.h: AnimEnded empty; factory state 10),
                   AIWarrior's magic (AICombat.cpp: StartedMagicAttack(Area), MagicAttack = one target,
                   MagicAttackArea = the 5x5 diamond on the staggered grid with its four range
                   patterns (a fifth / half / all of the damage), MagicAttackAreaFinished pays the
                   mana once, MagicHit / MagicBlock / MagicDeath with their delay timers (f90 / f94 /
                   f8c, already run by AIWarrior::Update), FindEpicenterPos), the tail of
                   AIWarrior::Hit (a buff potion's RANDOM_SPELL strikes the monster the hero hit),
                   AIPlayer::PlayBuffAnimation (wired in ItemHoverWindow's potion use),
                   ParticlePresets::OnSpell (PORT: only the returned delay per spell, 0 / .9 / 1 /
                   .25 / .25 / .75 / 1.75 / .25 / .5; the particles are milestone 6).
                   VERIFIED (map 15, RTK_SPELL in tools/test_hook.txt: slot 1..3 one target, 11..13
                   area, 99 = a 0.5 s delayed hit of spell 3; RTK_SPELLAT=<frame>): spell 1 takes 3 HP
                   and 100 mana; at 1 HP the monster dies (MagicDeath, dead=1); the area spell hits
                   once and pays once; a delayed hit lands after the wait; slot 2 without its quest
                   prints the original's "no AP" error. NOT seen: the hover texts and the spell
                   effects (only the last frame is screenshotted); nothing calls MagicAttack yet
                   except these tests (BaseCombat::SpellCast / the rage bar are 5j-3 / 5k).
              5j-3 [ ] the rage bar's rest, split in 5j-3a .. 5j-3c (2026-10-18: Update's rage part alone is
                   13 KB, OnAutoCast 3.5 KB):
              5j-3a [x] (2026-10-18) the bar's parts: RageButton (all 22 fields + LoadFrom) and
                   ManaProgress (+ LoadFrom; the clip rect is shared with its bar and number) loaded
                   in Init, rage_bar_bg's tap OnManaBar, OnShowManaPotion (the checks; its actions
                   are 5k / 5l / 5q), EnableFuryGlow, OnSpell (QueueSpellCast + Continue, the
                   second tutorial's step 0x9e -> 0x9f). VERIFIED: the bar loads without warnings;
                   OnSpell in the skeleton boss fight takes the fight from waiting (state 2) to
                   casting (10) and back. NOT seen: the bar's look (unchanged: Update draws nothing
                   of it yet).
              5j-3b [x] (2026-10-18) OnAutoCast (a rage button's tap: no mana -> OnShowManaPotion / the
                   "SPELL_NO_MANA" hint, locked -> the level / quest hint or TaskInfoWindow, castable ->
                   the spell marker flies from the button, in the arenas a random opponent is picked),
                   OnBotAutoCast, HelpWithSpell (arrow / popup), the buttons' click callbacks and
                   ShowMagicButton's arrow + lock. The flight's statics (g_fly*) are set; Update (5j-3c)
                   must move the sprite and cast on arrival. UNVERIFIED (5k): SpellMovement::Activate
                   (the arenas' aim); (milestone 6) sounds and ParticlePresets::SpellPreload; online
                   OG::MakeRequest is dormant. Note: which of activeMid / iconLocked means what is
                   Update's (5j-3c) to settle: the original's OnAutoCast casts when activeMid is hidden.
                   VERIFIED (temporary hook, boss fight on map 15): with the slot forced castable the
                   fire spell's marker sprite is made and its flight set to the boss; slots without
                   spells do nothing; before Update sets the looks all slots read "locked".
              5j-3c [x] (2026-10-18) BottomBattleWindow::Update in full (the original's order): the spell
                   marker's flight (a quarter circle in 0.5 s, then OnSpell; dropped when a window
                   opens, held for a popup), the mana bars (eased 13% per 1/30 s, a bar holds 300, the
                   "Fury: x/900" numbers, the tutorial's fixed 62 and its rise 0x9a -> 0x9d), the
                   background's glow, the stances (FirstLook = a boss before its fight object;
                   FightLook = ANY combat: attack / block, confirm / cancel while a spell is placed),
                   the centre button (net, else the run / walk switch + OnCenter's run part: "Mode:
                   Walking", the BOTTOM_BAR_* hint), the rage slots (spell icon, cost tiers orange 1..300 /
                   green 301..600 / red 601..900, lock with level or quest, new-spell marker, swirls,
                   connectors), the tutorial's arrow at the first slot (0x9d), and Click's slot drag
                   (a tap within 20 px = OnAutoCast; only from step 0x9e on, in a fight).
                   UNVERIFIED: SpellMovement::Activated is false until 5k (so also the 0x9e arrow and
                   the area spell's placing: g_placing); hurrying a search job with fury (5n); the
                   round mask on the centre icon; the hover texts.
                   VERIFIED (map 15, screenshots): the bar shows the run button, "Mode: Walking", the
                   mana bar "Fury: 125/900", fire (100) lit and lightning / arcane locked (200 / 300);
                   in the boss fight attack + block with "Stance: None" and OnAutoCast(0) sends the
                   fire marker flying toward the target.
              5j is DONE.
       5k [ ] spells 2, split in 5k-1 / 5k-2 (2026-10-18: the belt's buff row is a step of its own):
       5k-1 [x] (2026-10-18) SpellMovement (all 13 functions: the 5 x 5 footprint on the ground in three
              shades, tap / drag moves the selection inside the arena rect mBossMapPositions[8..11],
              Accept queues the cast with the tile, Decline) wired in main (init, press / release /
              move first in the chain, the pause save, the exit), ArenaCombat::Update (state 0xf at the
              turn's end), BottomBattleWindow (the large buttons' confirm / cancel taps, CastSlot's arena
              branch, the second tutorial's 0x9e arrow at the confirm button once the middle is on an
              enemy). PlaceMagicHoverWindow is DEAD (nothing constructs it): port_plan.txt. g_placing
              (0x618434) is never set true in 5.11 (nothing writes it): the original's drag-cast path is
              dead. Not wired (not ported yet): GameState::CleanBeforeLoading / HUDWindow::GameReload
              (both call Decline). Sounds ("ui_swing_out"/"ui_swing_in") are milestone 6.
              VERIFIED (boss fight on map 15, test hook RTK_SMOVE / RTK_SMACC / RTK_SMDEC / RTK_SMMOVE /
              RTK_SECOND / RTK_SMENEMY, screenshots): the footprint of spell range 2 appears around the
              chosen tile (red middle, orange ring, yellow edge), the large buttons show confirm /
              cancel with "Cast Spell", a tap inside the rect moves the selection (7,17 -> 9,18) and the
              footprint with it, a tap outside does nothing, Accept / Decline clear it and the buttons
              come back; Click does nothing while tutorial 0x80 and secondTutorial != 0x100 (as the
              original). NOT seen: an arena fight (nothing starts one before 5s, so the large buttons'
              taps, ArenaCombat's states and the 0x9e arrow with an enemy stay UNVERIFIED), the drag
              (Move), the "SPELL_MOVE" info text.
       5k-2 [x] (2026-10-18) the belt's fury flash and arrows: RunRageAnimation (BottomBattleWindow::Show
              at second-tutorial step 0x97: the potion icon of belt slot 1 flies to the bottom middle in
              1 s, step 0x99 -> 0x9a at landing), the flight's state and its Update (a shared picture:
              linear at step 0x99, else a quarter circle; the hidden slot icon returns),
              GetAdjustedBuffZoneLocation, ArrowAtSlot (its tap drinks the slot's potion, OnUseSlot) and
              OnShowManaPotion's belt arrow. VERIFIED (map 15, hook RTK_BELT1 / RTK_SECOND, screenshot): the
              potion icon is seen in flight and the tutorial goes 0x97 -> 0x99 -> 0x9a -> 0x9d. NOT
              seen: ArrowAtSlot (no test reaches a belt mana potion in a fight). UNVERIFIED: the flight's
              end at step 0x88 (CharacterInfoWindow::Hide, 5l); RaiseWindow stays with 5l (its timer is
              in Update and the character window raises the belt).
       5k-3 [x] (2026-10-18) the running potions' row: Shared::BuffHolder / BuffHolderManager (ten
              holders of Buff_holder_<size>.xml that glide to their place, fade in / out and show the time
              left as a clock; once a second they follow GameState::GetItemBuffRemainingTime of the type 0xe
              items), the engine's mask path (shader 5 = ring: the square is drawn where Sprite::extra >
              the mask; Render::SetColorMask on unit 1, Render::CreateCustomTexture, the clock's 24 x 24
              angle mask), wired in BeltBarWindow (manager (0x18, 2, -1, 0): create, Update, SetZ, Show,
              whose tail now also sets the Crafting button's lock / enabled look and preloads) and
              HealSquadWindow (0x18, 1, 0, 1 at buff_holder_01). VERIFIED (city, hook RTK_BUFF): two buff icons
              show above the belt in two rows, the clock's mask darkens the gone part (the black square is
              faint), a boss fight runs with them. NOT seen: HealSquadWindow's row (no test opens that
              window), a buff ending (the row closing up), the exact side the clock starts on.
              NOT in 5k: CharacterInfoWindow::ShowBuffAnimation (it reads the character window's own
              holder list and its hover: 5l), BeltBarWindow::RaiseWindow (its timer and the belt raised by
              the character window: 5l), BeltBarWindow's Deinit (the belt is never destroyed).
       5k is DONE.
       The hero's things:
       5l [ ] CharacterInfoWindow 1: equipment and belt (the inventory button, item moves). Split in
              5l-1 .. 5l-3 (2026-10-19: the window is 63 functions, UpdateContents alone 2700 lines).
       5l-1 [x] (2026-10-19) the window opens and closes: Init (Menu_character.xml: the figure, the 11
              equipment slots, 12 grid cells in a 2-wide scroller, 8 tabs, stats, set panel,
              customisation strip, the window's own buff row (0x18, 3, 0, 1), two item cards),
              Show (city only; tutorial only at 0x15; arena popup), Hide (the "NEW" items seen),
              OnTweenIn (the arrows / cards asked for by ShowInventoryItem), OnTweenOut, OnBack,
              OnTab, ShowInventoryTab, SetZ, GetZ, IsVisible, ZRange, CanRaiseBelt, RemovePreviewItems,
              ShowBuffAnimation, OnShop / OnSetInfo (their windows: 5q / 5m), the three holders'
              LoadFrom; GUI::SetHighDPIScaleFactor. The belt: OnInventory (the button, the 0xa3
              arrow), OnShowInventory, ShowInventory, its OnTweenIn, OnEmptySlot, RaiseWindow and its
              timer in Update (its only caller is online: PlayerCompetitionWindow), SetZ over the open
              window (CanRaiseBelt), Show clears the arrow id, the 0x88 flight's end hides the window.
              The portrait opens it (PlayerTopWindow::OnCharacter). VERIFIED (city, hook RTK_INV /
              RTK_INVTAB / RTK_INVHIDE / RTK_INVCHK, screenshot): the belt's inventory button and
              OnInventory open it (the XML layout, contents not filled yet), X and Hide close it.
              PORT: Click is a stand-in (the cards, the arrow, the root's buttons) until 5l-3.
              Not wired yet: fn.update (Update 0x285a1c: item timers, the drag, the buff row's place),
              Move, the cell / slot / customisation taps (OnItem, OnItemEquipped, OnPreviewItem,
              OnPreviewBuy), ShowInventoryItem (OnInventory's tutorial items 0x6b / 0x5b), the other
              callers of CharacterInfoWindow::Hide / Show / UpdateContents across the code (grep
              "CharacterInfoWindow" for the UNVERIFIED notes), ItemShopWindow::IsVisible in OnInventory.
       5l-2 [x] (2026-10-19) the contents: UpdateContents' window part (tab icons, header, the 11
              slots (the customisation tab: the customisation bindings of g_customOrder), the figure
              (PlacePlayerEquipment; the customisation tab wears only what is tried on), the 12 cells
              (owned / not owned pieces, prices in the customisation tab, "Owned: n", "x n" for packs,
              quality texts, lock / broken / level marks, "NEW" for 15 minutes once per opening), the
              set panel (the set with most bonus slots covered), the stats with the selected piece
              compared (red / white / green; long or short holders), the customisation strip and its
              crystal Buy), GetItemsForTabAndMode, IsValidItemForTabAndMode, GetItemAt (10: the
              profession tool), GetUniqueItemAt, GetToolOverride, UpdateItemTimers (the starter chest
              0x2b6, buffs), GetSelectedItem, IsItemEquipped x2, GetItemSlot, ItemHasSlot,
              GetItemPositionInCurrentTab, SetLastProfession, CheckIfItemImprovementLineHasItem,
              CompleteEquipTaskFor, CanMoveIntoEquippedBelt, HasMovedIntoInventory; MetaExpression's
              OnApplyItemEffect(int*, bool) (the 11-value effect record). Moved in from 5m: Sets (all:
              Init "sets.xml", the set state, CommitChanges, HasItemOrBetter ...) and Collections (all:
              Init "collections.xml" and the lookups), both loaded in main.
       5l-3 [x] (2026-10-19) the taps and moves: Update (card depth, buff row, scrollers, the glide with
              ease-out-back and the turning glow, the figure's equip flash / broken red pulse, the
              selected slot's pulsing frame, the buffs cut once a second), Click (cards, scrollers, the
              drag ends: a piece dropped on the figure is put on, a worn piece dropped off it comes off,
              an orb on the belt goes there), Move (30 px sideways picks a cell's piece up, 10 px up or
              down makes the press a scroll; 10 px picks a worn piece up), OnItem (card, compare card,
              scroll-in, 0x2f8 / 0x2f9 SelectHQWindow, 0x2fe random looks, customisation try-on),
              OnItemEquipped, OnPreviewItem, OnPreviewBuy (crystals, ConfirmPurchaseWindow skipped as
              the original asks), OnSelectRandomCustomizations, OnEquip (Equip / Unequip with glides,
              orbs to and from the belt), OnRepair, OnRemove, OnImprove (crafting: 5o), the admin
              OnAdd / Break / RemoveSelectedItem, MoveIntoInventory, ShowInventoryItem, ExecuteFirstHelp,
              CanTakeOnItem / CanTakeOffItem (moved from ItemHoverWindow); the belt's HasMovedIntoBelt,
              MoveIntoBelt, Move and its whole Click (a slot's item dragged into the grid), OnSlot with
              the window open. Wired: ItemHoverWindow's Equip / Unequip / Improve / Find (the windows it
              hides, RaidInfoWindow showing the item), the buff animation; OnInventory's tutorial items;
              BuildingHovers::OnCollect (the open window's belt, item 0x21d on map 0x52); Tasks' help for
              equip / improve / repair and the equip check; HealSquadWindow::OnRepair; SelectHQWindow's
              RemovePreviewItems; BottomBattleWindow::OnShowManaPotion.
              VERIFIED (city, hook RTK_ITEM / RTK_INV / RTK_DRAG / RTK_DRAG2 / RTK_EQCHK, screenshots):
              three new cuirasses with "New"; a tap opens the card with Equip and the stats show
              Initiative 50 -> 60 in green; dragging one onto the figure equips it (binding 2), the
              figure wears it and the set panel shows the Leather Set; dragging it off the figure
              puts it back in the grid; the card's Equip button equips; the customisation tab lists
              the looks with crystal prices, a tap tries the dark hair on (figure, strip, Buy 2), Buy
              takes 2 crystals and puts it on; a boss fight still runs. NOT seen: the belt drags, the
              tutorial paths (0xa4, 0x14 / 0x15), the starter chest timer, the random looks, sets with
              bonuses complete, the compare card. UNVERIFIED (milestone 5): the professions (5n: the tool
              slot, profession icons and point items), the item shop (5q), the crafting windows (5o).
              With no item tried on, the customisation tab shows the stats under the strip as the
              original's visibility rules say (compare with the real game).
       5l is DONE.
       5m [x] SetInfoWindow (Sets and Collections were done with 5l). Split in two:
         5m-1 [x] 2026-10-19: Init (tabs from Sets' sort order: each tab the next active set and the
              inactive one it replaced), the five LoadFroms, Show / Hide / OnBack / tweens / SetZ /
              ZRange / Click / Deinit, OnTab, OnInfo (the "?" popup), SelectSet, SelectItem,
              ShowNextItemHelp (+ OnTweenIn's arrow), OnItem (owned: the character window on the
              piece; missing: the mission map that drops it, else the popup "HELP_FIND_SET"),
              GetPlayerItemOrBetter now follows the Collections chain. Wired: CharacterInfoWindow::
              OnSetInfo, TaskCompleteWindow::OnTweenOut (a completed set), TaskSubtask::OnHelp
              (kCollectSet), main's Queue. VERIFIED in screenshots (hook RTK_SETINFO): the window
              opens with 7 tabs and 3 flags of 3 slots, "?" shows the explanation popup, the close
              button hides it. UpdateContents is an empty stub, so the tabs and slots are blank and
              OnItem does nothing yet (its slot lists g_slots / g_slotItems are filled there).
              UNVERIFIED: OnItem's item shop (5q) and chests (5p) branches.
         5m-2 [x] 2026-10-19: UpdateContents (@0x33b6a0, read from the asm: the decompile is
              garbled): the tabs (icon: the set's first piece that is not a ring / amulet; the
              selected one raised 4 px), "SET_TITLE", the set's name, the slot lists of each bonus
              (and of the replaced set), per flag the name in its lowest item class's colour, the
              slots centred, each slot's owned piece (or one it improves into, or the replaced
              set's), worn ticks, find / all-items marks (only once the shop is open), the bonus
              state (COLLECT / EQUIP_TO_ACTIVATE, ITEM_BONUS_ACTIVE), the red / gold flag, and up to
              three stat lines ("+%d %s", ITEM_BONUS_PARTY, the squad's lines) centred. Also
              MetaExpression::OnApplyItemSquadEffect (@0x1d11fc). VERIFIED in screenshots (hooks
              RTK_SETINFO, RTK_SETGIVE gives the set's three Basic pieces and wears two): Leather
              Set with 3 / 2 / 2 slots and its stat lines, the Iron Set tab, the Basic flag red with
              a gold border, two worn ticks and "Equip to Activate", a tap on an owned piece opens
              the character window with the arrow on it. NOT verified: the find marks and
              OnItem's mission map (needs the shop open, secondTutorial 0x100), replaced sets, the
              party line.
       5m is DONE.
       5n [ ] Professions, LevelInfoWindow (56 functions: Professions 24 + GameState's two point
              functions, LevelInfoWindow 32; UpdateContents alone is 15 KB). Split 2026-10-19:
         5n-1 [x] 2026-10-19: Professions (all; professions.xml loaded at 19/46 with (0x7e7, 0), read
              from the asm), GameState::GetProfessionPoints / AddProfessionPoints. GetProfessionTool
              and GetItemProbability / GenerateItem's chance were read from the asm (owned tools:
              per item first the shortest GATHER_TIME, then the highest GATHER_CHANCE; the chosen
              tool wins; the chance (int)(0.5 + (1 - ((1 - r/100) + ((1 - T/100 or 0.7 without a
              tool) + (1 - level rate/100)) / 2) * modifier) * 100)); the rolls use the original's
              own LCG seeded with the time. Wired: CharacterInfoWindow (the tool slot's tool, the
              profession badge on items, a point item's tap turns all of them into points),
              Items::ItemInfo::IsProfessionTool / IsLockedByReputation, Tasks (CanBeCompleted, a
              reward point item gives points, the reputation step), TaskInfoWindow's reputation count.
              VERIFIED (hook RTK_PROF + RTK_INV): 7 professions with their levels, tabs and objects;
              with Rusty and Iron Pickaxes owned and 60 miner points (level 2) the tool is the Iron
              Pickaxe and marble's chance 73 (by hand: 72.5 + 0.5); the character window's tool slot
              shows the herbalist's Rusty Sickle with "Equipped", the Iron Pickaxe locked at level 6.
              UNVERIFIED: AddProfessionPoint's level-up window (CraftSuccessWindow, 5o); Deinit is
              not called (the port skips the systems' Deinit at exit, as for the others). NOTE:
              rep_agressor, rep_defender, the tamer and valor have unlock_quest '0', so IsUnlocked
              needs task 0 completed (as in the original's code): compare with the real game.
         5n-2 the profession placeholders, split in two (2026-10-19):
         5n-2a [x] 2026-10-19: RaidInfoWindow (a profession item's drop chance from its gathering
              chance, the list skips locked ones, the REPUTATION cost's icon and level),
              CampaignInfoWindow (the list skips locked profession items), BossOutroWindow (the
              arena's Bloodletters bar: points before -> plus the map's 0x1ba items, filling in 2 s,
              "gained" / "next rank"), BottomBattleWindow (the centre button hurries the hero's search
              job with fury: search_to_fury_mult per second left; icon = the best tool, read from the
              asm), PlayerBattleInOutWindow (the PvP outro's valor bar, its texts and pulse, from the
              asm), PlayerTopWindow (the valor bar: profession 7's points between its ranks),
              ChoosePathWindow (the PvP holder's rank and level), NotEnoughWindow (the profession
              requirement and its line "REQUIRES_REPUTATION_LEVEL"), ItemHoverWindow (the set line,
              the rank line with the profession's icon, the reputation button; OnRemove / OnRepair /
              SetItem's tool tests are Professions::IsProfessionTool, GATHER_CHANCE only, as in the
              original: fixed), FoundItemsWindow / BuildingHovers (point items give points; their
              flight goes to the crafting badge).
              VERIFIED (hook RTK_PROF + RTK_INV + RTK_VALOR=120, a tap on the Iron Pickaxe): the card
              shows "Novice" with the miner icon and "Gather chance: Medium"; the top bar shows valor
              120/250. NOT seen: the arena / PvP bars (no arena yet), the fury hurry, the requirement
              line. UNVERIFIED: BeltBarWindow::ShowProfessionPoint (the belt's crafting bar, 5o);
              ChoosePathWindow's arena holder (5s); NotEnoughWindow's kProfession Find
              (LevelInfoWindow / CraftItemWindow, 5n-5 / 5o).
         5n-2b [x] 2026-10-19: gathering on mission maps: Map::Decor::OnSetup's CRAFT nodes (after the
              regrow time reset to 0x5e3, then GenerateProfessionObject for the map: the map's
              quality is the percent chance, the object by tileset; none: hidden), the tile blocked
              and "bless"; GetProgressMessage (<name>_action), GetRequiredItem / IsLocked (no owned
              tool: the worst tool is asked for), CanDropItem (Locations::CanCraftNodeDropItem),
              Finish's gathering (GenerateItem drop with "+1 <profession>" and a point, or
              "SEARCH_NOTHING"; the tool loses death_penalty, "DURABILITY_PENALTY" at 0);
              BuildingHovers::OnDecorClick (the job takes the tool's GATHER_TIME, else 2 s).
              BossIntroWindow's reputation screens (SetReputation, from ArenaCombat (5s): an arena
              event lost): Show (the event's consolation loot: wave_lost_base / _per_wave items 0x1ba,
              arena_dropback_defeat of the gold spent, arena_boss_dropback items 0x1bb; FoundItemsWindow
              with ARENA_WAVE_DEFEAT_HEADER, the band slides down; ArenaTurnWindow hidden first),
              Update (the bar), UpdateContents, Hide (slides up), Click (to the loot strip), OnTweenOut
              (the event ends: times, tasks reset, PostCombat / EndActiveCombat, back to the return
              map); GameState::SetEventEndTime.
              VERIFIED (hooks RTK_PROF, RTK_HERB=2200 (task 1443 + herbalist points), RTK_GOMAP=71,
              RTK_CRAFT lists the nodes, RTK_GATHER + RTK_GFIN=6 finishes the nearest node 6 times):
              map 71 (quality 40, its items need herbalist 4 / miner 3): at herbalist 0 every node
              hides; at 4 about 40% become herbs (1508), unlocked with the owned sickle, with the
              action text; 6 finishes: one herb dropped with "+1 Herbalist Profession" (2200 -> 2201),
              "There's nothing here..." for the others, the sickle 12 -> 6. NOT seen: the walk and the
              job timer (the map's NPC talk interrupted the walk), the regrow, the reputation screens
              (need the arena, 5s), DURABILITY_PENALTY. UNVERIFIED: ShowProfessionPoint and
              CraftSuccessWindow (5o); WorldHintHoverWindow::OnClose's task 0x5a8 (5o); the sounds (6).
         5n-3 [ ] LevelInfoWindow opens: the LoadFroms, Init, Show / Hide / OnBack / tweens / SetZ /
              ZRange / Click / Move / Deinit / IsVisible, OnTab, OnLevel; PlayerTopWindow's level button.
         5n-4 [ ] LevelInfoWindow::UpdateContents (@0x2ed6d0, 15 KB: read from the asm).
         5n-5 [ ] LevelInfoWindow's taps and helps: OnItem, OnBuilding, OnUnlockedItem, OnLevelHolderDown /
              Up, OnShop, OnActualHelp, OnHelp, HelpWithArena / HelpWithPvP, Update, OnTweenOut; wired:
              ChoosePathWindow (helps, profession level and rank), TasksHelp's kBalanceLevel,
              ItemHoverWindow / FoundItemsWindow's LevelInfoWindow calls.
       5o [ ] crafting: CraftingWindow, CraftItemWindow, CraftSuccessWindow.
       5p [ ] chests (Chests, ChestsWindow, ChestOpenedWindow). DECIDED 2026-10-18: keys (the chests'
              "required items") can no longer be bought; a map that holds a key gives it with a 1/4
              chance on completing the map, or, on a map with a boss, a 1/2 chance from the boss
              instead. Chests give crystals (2026-10-07). Real-money chests go; confirm at the start
              whether crystal-priced chests stay buyable.
       5q [ ] ItemShopWindow, gold/crystal prices only. PlayerWeakWindow is REMOVED (user 2026-10-18).
       5r [ ] DailyBonusWindow (the offline daily crystals), Tavern, ResourceHelper, Notifications,
              the calendar events.
       Arena and PvP (offline, room left for a server). DECIDED 2026-10-18: opponents are generated
       rivals (names, levels and gear scaled to the player, built like the ambush's rival); ranks
       and valor kept locally; a server could replace the generator later.
       5s [ ] the arena: ArenaEventWindow, ArenaDifficultyWindow, BottomArenaWindow, ArenaCombat's rest.
       5t [ ] PvP 1: PvP (the opponent list), AsyncBaseCombat, AsyncCombat.
       5u [ ] PvP 2: AsyncCombat_v2, PlayerInfoWindow, PlayerNameHoverWindow.
       5v [ ] PresentsContainer. DECIDED 2026-10-18: ported but dormant (no gifts arrive offline); the
              warehouse keeps working for everything else that uses it.
       Tools and leftovers:
       5w [ ] the admin mode: EditorConsole, Editor, DebugViewer (store items for free).
       5x [ ] leftovers: GameState (51), Entity (16), Map (9), AIWarrior (9), EntityData, MetaExpression,
              BottomScreenshotWindow (+ the screenshot help notes), roads (BuildingMovement kRoad), and
              the ~100 UNVERIFIED notes without a milestone (sorted into fixes or 6/7).
       5y [ ] close: inventory --functions 5 empty, no "milestone 5" note, a long play test, STATUS.
6. [ ] Sound and effects: music/SFX playback (the GUI and game sound hooks), particles, weather,
       screen effects.
7. [ ] Mac release and polish: .app bundle, settings/persistence paths, languages, Retina/fullscreen,
       (user 2026-10-18) a frame-rate cap option (e.g. 30 / 60 / display): the port runs at the
       display rate (120 Hz here) and feels faster than the original on phones, though its timing
       code matches the original (real-time dt, cos(t*15)*8 arrow bob, 0.3 s sepia);
       performance pass, full playthrough check against the original.
   Every class of the binary is placed in one of these (or marked online-only) in
   docs/port_inventory.md; `python3 tools/inventory.py` gives the coverage. Re-check after each step.

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
