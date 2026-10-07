# Port inventory: everything in libkingdom.so 5.11 and where it goes

Built on 2026-10-07 from the binary, not from memory: every top-level class in `out/decomp.c`
(258 of them, about 9,000 functions after leaving out library code and container templates),
plus the game's free functions and the data files. `python3 tools/inventory.py` prints the current
ported/total count per class; `--missing` lists the classes with nothing ported. **Rule: every class
it prints must appear in this file.** When a step finishes, re-run it and fix this file if anything
is unplaced.

Milestone numbers below follow the proposed layout (section 1). Numbers like `12/40` are
ported/total functions at the time of writing.

## 1. Milestone layout

| # | Milestone | Status |
|---|---|---|
| 1 | Map rendering | done |
| 2 | GUI, input, dialogs | done |
| 3 | City economy (buildings, workers, shop, farms, saves) | done |
| 4 | Hero, army, quests, items, decoration jobs, campaign maps, campaign combat, new-game tutorial (4a-4h) | in progress (4a.1 done) |
| 5 | The rest of the game: spells, boss/siege fights, global map, hire/heal army, item shop, chests, crafting, collections, sets, tavern, daily bonus, events, arena and the offline replacements of the online features | planned |
| 6 | Sound and effects (music, SFX, particles, screen effects) | planned |
| 7 | Mac release and polish (settings persistence, .app, Retina, playthrough check) | planned |
| — | Online only: stubbed, not ported (a future server project may revive some) | — |

## 2. Already ported or partly ported (the remainder is placed below)

Done or nearly done (re-run the script for exact counts): AI (waypoints, paths), AIFarmerBig,
AIFarmerSmall, AIGoblin, AIPatch, AIWorker, Animation, AnimationController, Background, BaseAI,
TargetedAI, BaseHoverWindow, BubbleHoverWindow, BuildProgressHoverWindow, BuildingHoverWindow,
CastleHoverWindow, EmptyHoverWindow, FactoryHoverWindow, FarmRestoreWindow, LivingHoverWindow,
PatchProgressHoverWindow, PlaceBuildingHoverWindow, ResourceHoverWindow,
ResourceRestoreHoverWindow, SleepingHoverWindow, StorageHoverWindow, TaxesHoverWindow,
PatchAnimationController, BottomCityWindow, BottomFarmWindow, BuildingMovement, BuildingPlacement,
CallbackCenterOnWorker, CallbackRemoveWorker, CallbackUpgradeBuilding,
CallbackUpgradeBuildingContinuation, CityRenameWindow, ConfirmPurchaseWindow, Contracts,
ExchangeWindow (as the offline exchange), HiddenObjects, LandWindow, LandExpandedWindow,
LevelUpWindow, MapMovement, NotEnoughWindow, PopupWindow, PopupSelectionWindow, SaveManager,
Setting, ShopWindow, SoldierSlots, SoldierPool, BaseSquad, PlayerSquad, EnemySquad, TextInput,
TextfieldCallback, TextStyleManager, TopCityWindow.

Not to port: FarmGrowHoverWindow (created but never shown in 5.11, see 3f.4).

Small helpers, ported with their first caller: BRand (random numbers), MapObject (4/15, the
building/decoration base), StringHelper, WSplitter (wide-string Splitter), ClearPointerVector.
Library code the script also lists: pugi (pugixml, used as a library), __cxxabiv1, _JNIEnv
(Android).

Partly ported, with the rest in these milestones:

| Class | Ported | Remaining work goes to |
|---|---|---|
| GameState | 78/316 | tasks/items/combat/regeneration state 4; events, arena, PvP-tutorial, chests 5; online sync: stub |
| Map | 191/406 | spawns, portals, fog, persons, static meta 4; boss rewards 4g/5; global-map travel 5 |
| Entity | 110/287 | combat stats, attacks, death, patrols, talk tasks, projectiles 4; spells 5; particles/sounds 6 |
| EntityManager | 24/66 | aggro, enemies, talk tasks, task events 4 |
| EntityFactory / EntityData | 9/22, 13/30 | arena/tamed entities 5; the rest 4 |
| BuildingHovers | 56/96 | world dialogs, item drops, quest arrows 4; spell targeting 5 |
| AIBaseState | 31/75 | combat/aggro/portal/decoration virtuals 4 |
| BaseCombat | 6/64 | 4g |
| HUDWindow | 17/50 | task arrows, map names, battle mode 4; global map/arena 5 |
| CastleTopWindow | 9/29 | featured quests 4 |
| PlayerTopWindow | 6/18 | HP/XP/energy, item animations 4 |
| BattleBarWindow / BeltBarWindow | 5/20, 6/35 | 4g |
| TaskHolderWindow | 9/24 | 4d |
| Shared | 35/75 | task/item/warrior holders 4; chest/raid/craft holders 5 |
| WindowManager | 31/71 | remaining queue functions with the windows that need them |
| GUI | 111/169 | with the windows that need them |
| Render | 44/175 | animated camera (CenterOn tween) 4; particles/filters 6; Retina/fullscreen 7 |
| Resources / IconManager | 5/15, 3/10 | async/online images: stub; the rest with their callers |
| FileManager | 13/90 | storage/paths 7; packs/downloads/online: stub |
| SystemFuncs | 6/96 | settings persistence 7; device/online functions: stub |
| Timer | 6/17 | TimeCompensationBlock 6/7 |
| SettingsWindow | 33/47 | language selection 7; online buttons: stub |
| MetaData | 10/16 | 4c |
| Splitter | 1/13 | with its callers (4) |
| StringTable | 10/16 | languages 7 |
| ExchangeWindow | 25/37 | the rest are real-money paths (removed by design) |
| DecorationHoverWindow | 9/13 | 4e |

## 3. Milestone 4 (hero, army, quests, items, jobs, campaign, tutorial)

- **4a hero and army on the city map**: AIPlayer, AIWarrior, AIWarrior_v2 (check which one 5.11 uses),
  AIStateFactory, the hero's world taps, App (7: the EntityManager owner; folded into globals).
- **4b items**: Items, Items::ItemInfo/ItemData, items.xml, item_packs.xml, ItemHoverWindow,
  UseItemHoverWindow, FoundItemsWindow, NeedItemWindow, CharacterInfoWindow (equipment, stats),
  PlayerInfoWindow, PlayerNameHoverWindow, BuildingHovers::ItemDrop's item kind,
  Map::ItemPlaceContinuation.
- **4c quest engine**: MetaExpression (the parser is in 4b.1), Tasks (+TaskInfo, TaskSubtask), tasks.xml, Tags (ported in 4b.1),
  Override, Bonus (check: task/level bonuses), the task events in Entity/EntityManager/Map.
- **4d quest UI**: TaskHolderWindow (+QuickTaskWindow), TaskInfoWindow, TaskCompleteWindow,
  TaskListWindow, Shared::TaskWindow, CastleTopWindow::FeaturedQuestHolder, LevelInfoWindow,
  TaskQuickCompleteWindow (ask the user: crystal skip), HelpWindow.
- **4e decoration jobs**: Map::Decor (77; 10 ported), DecorAnimController, BaseAnimController,
  BuildingAnimController, Save::DecorUpdate's in-game part, CallbackAssignWorker.
- **4f campaign maps**: Locations (+LocationInfo, locations.xml), SpawnPoint, Portal, Fog,
  Map::Person/PersonData, PersonHoverWindow, TalkHoverWindow, TapHoverWindow, HintHoverWindow,
  WorldHintHoverWindow, PlayerHintHoverWindow, AIEnemy, EventManager<Entity/Building>,
  ChoosePathWindow, CampaignInfoWindow, CampaignCompleteWindow, CampaignPerfectedWindow,
  ChapterWindow, LoadingScreenWindow (map loads), GameState::RefreshOfflineGoblins.
- **4g campaign combat**: CombatManager, BaseCombat, EasyCombat (+legacy Squad), AggroCombat,
  RangeAggroCombat, EasyAI, Projectile/ProjectileData (projectiles.xml), BattleBarWindow,
  BeltBarWindow, BottomBattleWindow, PlayerBattleInOutWindow, HealthbarHoverWindow,
  HealthbarTinyHoverWindow, HealHoverWindow, HealSquadWindow, PlayerWeakWindow, HP regeneration
  and death (ask the user how revive works offline).
- **4h new game**: LoadSavedGame's new-game branch, TutorialWindow (+UpdateCity/UpdateSecond),
  TutorialSender (no-op offline), SelectHQWindow + Select_HQ (gender/hero choice; its Facebook
  buttons: stub), FirstCityEnterEffect, OnTutorialFarmerWomanAction, ReloadSavedGame/GameReset.

## 4. Milestone 5 (the rest of the game)

- **Army**: HireTroopsWindow (buying soldiers; needed early, may move into 4g if the campaign
  requires it), RepairWindow.
- **Spells**: Spells (spells.xml), SpellMovement, AISpell, PlaceMagicHoverWindow, the gesture
  recognizer (dollarRecognize & co., drawing spells), Particles::SimpleSpellParticle/MageParticle
  hooks.
- **Boss and siege fights**: BossCombat, BossCombatWaves, BossIntroWindow, BossOutroWindow,
  BossTimeHoverWindow, BossIntroEffect, FastBossIntroEffect, SiegeCombat, AISiegeAttacker,
  EventManager<SiegeCombat>.
- **Raids (hunts/grinding)**: StartRaidingWindow, RaidInfoWindow, RaidCompleteWindow.
- **Global map**: GlobalMapWindow (+locations), map travel.
- **Economy extras**: ItemShopWindow, shop_packs.xml (check for real-money offers: remove), Chests
  (chests.xml), ChestsWindow, ChestOpenedWindow, CraftingWindow, CraftItemWindow,
  CraftSuccessWindow, Professions (professions.xml), Collections (collections.xml), Sets
  (sets.xml) + SetInfoWindow, Tavern (tavern.xml), DailyBonusWindow, SultanArrivingWindow
  (timed event), ResourceHelper.
- **Arena and PvP, offline versions (ask the user first)**: ArenaCombat, ArenaDifficultyWindow,
  ArenaEventWindow, ArenaTurnWindow, BottomArenaWindow, AsyncBaseCombat, AsyncCombat,
  AsyncCombat_v2, PvPArenaCombat, AIPlayerBot, CombatTestBot, PvP (the PvP tutorial players are
  local bots), PlayerCompetitionWindow (online hub: only its offline-usable parts).
- **Presents** (PresentsContainer, ask: gifts from friends), **Notifications** (local reminders).
- **Admin/debug mode (user request)**: EditorConsole's commands (CmdAddItem, CmdAddResource,
  CmdCompleteTask, CmdUnlockAll, CmdGodMode, CmdOpenMap ...) are the original's own cheat
  console; the offline admin mode can be built on them. Editor/DebugViewer: dev tools, optional.
- BottomScreenshotWindow (screenshot sharing: offline = save to disk, or drop: ask).

## 5. Milestone 6 (sound and effects)

Sounds, SoundsManager (+CSoundEvent), the music/SFX hooks marked UNVERIFIED everywhere,
Particles (all 13 particle types), ParticlePresets, weather and screen effects, GUI::TextCache,
BuildingHovers::TextCache (caches).

## 6. Milestone 7 (release)

SystemFuncs settings persistence, StringTable languages (LocalizedStrings*.xml), SettingsWindow
language choice, LoadSampling (load progress), Timer::TimeCompensationBlock, PlayerProfileManager
and SwitchProfilesWindow (several local save profiles: ask whether wanted), ErrorReporter
(logging), Render remainder (Retina, fullscreen), .app bundle.

## 7. Online only: stubbed, not ported

CMyServer, CConnection, CGZIP2AT, NeighboursServer, NeighboursServerNS, Neighbours, MyNeighbours, MyFacebook,
SocNets, SocialConnectWindow, FacebookSyncWindow, VisitFriendWindow, FriendInfoHoverWindow,
SelectFriendsWindow, Window_switch_profiles (Facebook profiles), WallPost, ViralFeatures, OG, OG2,
ServerFlags, Billing, CMyPayment_CBMoneyLog, CMyPayment_PurchaseLog, CMyPayment_PurchaseLogRecord
and CBMoneyLogRecord (real money: removed by design), CouponEntryWindow, CouponGetWindow,
CouponSetWindow, DownloadManager, AsyncImageLoader, Expansions and
ExpansionDownloadWindow (the port ships every pack), LoadingInternetWindow, ToS, Save:: (163
functions: the save diff/minify system for server sync; the local save is SaveManager, done),
curl, the Android/iOS JNI glue, the server URL helpers.

## 8. Data files (res_files/1Original)

| File | Milestone |
|---|---|
| buildings, decors, deliveries, dynamic_config, persons, shoptabs, AllAnimsFrames | done |
| items, item_packs | 4b |
| tasks | 4c |
| locations | 4f |
| projectiles | 4g |
| spells, chests, collections, sets, professions, tavern, shop_packs | 5 |
| LocalizedStrings* | 7 (English already loads) |
| files_in_packs_list | pack index (done through FileManager) |
| cars, eras, roads, terrain | not referenced by the 5.11 code: leftovers, nothing to port |
