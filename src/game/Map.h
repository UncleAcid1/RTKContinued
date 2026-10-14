// Map: grid, patches (land areas), decorations and buildings, and their sprites.
// Port of the Map namespace (libkingdom.so 5.11): load path Map::Load @0x1c3518 -> LoadPlayer @0x1c0cd8
// -> Patch::Load @0x1e3870 (LoadDecors @0x1e271c, LoadBuidings @0x1e2bdc), CreateRandomDecors, UpdateAreaBorders.
// Buildings are game/Building.h. Decorations' gameplay state, workers, AI and quests are not ported yet.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "game/Building.h"

namespace Render { struct Sprite; struct Texture; }
namespace GameData { struct DecorData; }
namespace SaveManager { struct SaveBlock; }

class MetaExpression;
class MetaData;
class Entity;
struct SpawnPoint;
struct Portal;
namespace Contracts { struct Contract; }

namespace Map {

struct Patch;

struct Decor;

// BaseAnimController<Map::Decor> (0x1c bytes, Update @0x1329ac): plays the frames of an animated
// decoration's sheet, 1.5 frame times per frame.
struct DecorAnim {
    int frame = 0;                     // +0x04
    int shownFrame = -1;               // +0x08 the frame its sprite shows
    Render::Texture* texture = nullptr;   // +0x0c
    Decor* decor = nullptr;            // +0x10
    float timer = 0.f;                 // +0x14
    bool paused = false;               // +0x18
    void Update(float dt);
};

struct Decor {
    Patch* patch = nullptr;            // +0x04
    uint8_t x = 0, y = 0;              // +0x0c, +0x0d
    uint32_t id = 0;                   // +0x18
    bool mirrored = false;             // +0x1c (map flag bit 0)
    bool visible = true;               // +0x1d
    std::string metaText;              // the text of its MetaExpression (kept for the save)
    MetaExpression* meta = nullptr;    // +0x20 its quest behaviour (see OnSetup), null without one
    bool destroyed = false;            // +0x40 Destroy (only the siege system, dead in 5.11, calls it): Map::Update removes it
    bool disabled = false;             // +0x41 DISABLE_AFTER_* happened: no longer interactive
    mutable const GameData::DecorData* data = nullptr;  // +0x3c (GetData fills it on first use)
    bool fake = false;                 // +0x43 random decoration (Patch::AddRandomDecors)
    // +0x44 (map flag bit 1; also set on unowned city patches and in the tame tutorial). Decor::IsFake
    // is +0x43 || +0x44. UNVERIFIED: only loaded and saved so far.
    bool hidden = false;
    bool trapOn = true;                // +0x42 a trap's phase (TRAP_PATTERN): it hurts while set
    // +0x48: when its taxes (DecorData collectTime) or the job on it (workTime) started; a trap's
    // last hit.
    uint32_t collectStart = 0;
    uint32_t f4c = 0;                  // +0x4c how many times it can still be used (saved byte)
    uint32_t f50 = 0, f54 = 0;         // +0x50 the seconds its job takes; +0x54
    std::string resourceText;          // +0x58 "TYPE=n|..." the job's price, parsed into cost
    int cost[11] = {};                 // +0x5c per resource type (ReturnCost)
    int linearX = 0, linearY = 0;      // +0x10 +0x14 (TileCoordinatesToLinear of x, y; UpdateImage)
    // +0x24..+0x38 FindBaseCoordinates: the base point's x three times (base, min, max over the
    // sprites) and its y three times (base, max, min).
    float baseX = 0.f, minX = 0.f, maxX = 0.f, baseY = 0.f, maxY = 0.f, minY = 0.f;
    Render::Sprite* sprite = nullptr;  // +0x88
    Render::Sprite* extraDecors = nullptr;   // +0x8c more sprites of it, chained by Sprite::next
    std::unique_ptr<DecorAnim> anim;   // +0x98 while its image is an animation sheet
    Portal* portal = nullptr;          // +0x90 the way to another map it is (Map::LinkPortals)
    Entity* worker = nullptr;          // +0x9c the hero or goblin doing its job
    Entity* particles = nullptr;       // +0xa0 its particle entities, chained by Entity::nextParticle
    std::vector<Entity*> attackers;    // +0xac
    int hp = 0, hpMax = 0;             // +0xb8 +0xbc
    bool removed = false;

    Decor() = default;
    Decor(const Decor&) = delete;
    Decor& operator=(const Decor&) = delete;
    ~Decor();
    // PORT: sets metaText and parses it into meta (the original parses where it loads the text).
    void SetMeta(const std::string& text);
    bool IsFake() const { return fake || hidden; }   // @0x130e20
    uint32_t GetStartQuestID() const;                // @0x1316b8 START_QUEST
    uint32_t GetEndQuestID() const;                  // @0x131688 END_QUEST
    // @0x134100 / @0x133fec: the tiles its BT list names are blocked and lose their waypoints, or
    // are freed and get them back.
    void SetBlockedTiles();
    void RemoveBlockedTiles();
    // The quest reactions of its expression (Map::OnTaskCompleted ... call them for every decoration).
    void OnSetup();                                  // +0x34 @0x139190 after the map is loaded
    void OnTaskCompleted(uint32_t task);             // +0x38 @0x138ae4
    void OnSubTaskCompleted(uint32_t subtask);       // +0x3c @0x138430
    void OnInteract(uint32_t object);                // +0x40 @0x1341d4
    // @0x131234: the data of `id`, looked up on first use. UNVERIFIED: an id that is a building's
    // (the original then looks it up and drops it) gives null.
    const GameData::DecorData* GetData() const;
    // @0x13120c: its data's icon_y (without data, that of a building with its id; else 0).
    int GetIconOffsetY() const;
    void GetStartTile(int& tx, int& ty) const;       // +0x18 MapObject @0x1c7658
    void GetBuildZone(int& w, int& h) const;         // +0x1c MapObject @0x1c76e4
    void ToggleMirror() { mirrored = !mirrored; }    // +0x28 MapObject @0x1c7724
    // +0x20 @0x134554: every tile of the footprint is free (a fake decoration counts as free when
    // ignoreFake; the tile's virtual decoration never blocks), on the grid and not on unowned land.
    bool CanBePlaced(bool ignoreFake) const;
    void UpdateImage();                              // +0x0c @0x134c14
    void GetBasePoint(float& X, float& Y) const;     // PORT helper (needs data)
    void UpdateMapLink();                            // @0x1346b8
    // +0x2c @0x1344b0: the sprite has a pixel at the world point; a plain decoration (no quest, portal,
    // worker or collect time) only counts when `any`.
    bool Contains(int x, int y, bool any) const;
    void ReplaceWith(uint32_t id);                   // @0x131f08
    // @0x131284: the crystal speed-up of a decoration's job: collectStart = now + ~f50.
    void SpeedupDecoration();

    // ---- the job on it and its expression's answers (game/MapDecor.cpp)
    void AddExtraDecor(Render::Sprite* s);           // @0x130b88
    void RemoveExtraDecors();                        // @0x13297c
    bool HasEntity(const Entity* e) const;           // @0x130b98 its worker or one of its particles
    void RemoveHoldOfEntities() { particles = nullptr; worker = nullptr; }   // @0x130bd8
    void FindBaseCoordinates();                      // @0x130be8
    Render::Sprite* GetSpriteList() const { return sprite; }   // @0x130e18
    bool WorkerAssigned() const { return worker != nullptr; }  // @0x130ea4
    Entity* GetWorker() const { return worker; }     // @0x130eb4
    bool IsDestroyed() const { return hp < 1; }      // @0x130f40
    void OnAttackedBy(Entity*) {}                    // @0x130f54
    void GetCenterWorldXY(int& x, int& y) const;     // @0x130f58
    // @0x130fa0: e stands within range (at least 1) linear tiles of the footprint.
    bool IsEntityInRange(const Entity* e, int range) const;
    bool WorkerIsWorking() const;                    // @0x1311e8 its worker's AI says so
    bool WorkFinished() const;                       // @0x1312a4 workTime after the start has passed
    uint32_t GetReplaceWithTarget() const;           // @0x131658 REPLACE_OBJ
    const char32_t* GetName() const;                 // @0x1316e8
    void WorkEnded();                                // @0x1317a4
    // @0x1317b4: (x, y) is one of its quest, teleport or action tiles (START_QUEST_TILE,
    // END_QUEST_TILE, END_SUB_QUEST_TILE, TELEPORT_TILE, AP, TP).
    bool HasTriggerTileAt(int x, int y) const;
    bool HasBlockTileAt(int x, int y) const;         // @0x131aa0 BT
    unsigned GetActionPointCount() const;            // @0x131b48
    // @0x131ba4: where its user stands: AP's i-th tile, TP's tile, else (a craft node or a typed
    // chest) the first neighbouring tile with a waypoint, else its own tile. (0, 0) without any.
    void GetActionPoint(int& x, int& y, int i) const;
    const char* GetGoblinSearchAnimation() const;    // @0x131e28 GOBLIN
    const char32_t* GetHintMessage() const;          // @0x131e58 HINT
    const char32_t* GetBlockMessage() const;         // @0x131e90 BT's leading text
    const char32_t* GetProgressMessage() const;      // @0x131f74 PROGRESS_MSG
    // @0x132040: the item its use takes (REQ_UNLOCK_ITEM), 0 while its unlocking quest or step is
    // open or without one.
    uint32_t GetRequiredItem() const;
    // @0x1321b8: its unlocking quest (REQ_UNLOCK_QUEST) or step is open, or the player lacks its item.
    bool IsLocked() const;
    // @0x13234c: MESSAGE's text; else, when unlocked by its quests but the item is missing,
    // "MSG_MISSING" with the item's name; else empty.
    std::u32string GetMessage(unsigned size) const;
    // @0x132ad8: INTERACT_ANIM's animation name (its number into time); null without a valid one.
    const char* GetPlayerSearchAnimation(unsigned* time) const;
    void ReturnCost();                               // @0x132b8c the job's price back to the player
    void AssignWorker(Entity* e);                    // @0x1336f0 (dropped again if its AI refuses)
    void RemoveWorker();                             // @0x133748
    void RemoveGoblinWorker();                       // @0x134b94 forgets a worker that is not the hero
    bool CanDropItem(uint32_t item) const;           // @0x133dc0 DROP or DROP_AFTER_QUEST names it
    void AddAttacker(Entity* e) { attackers.push_back(e); }   // @0x13b650
    void RemoveAttacker(Entity* e);                  // @0x13a388
    // @0x13b99c: the world point of the tile half its depth back from (x, y).
    void GetCenterPoint(int& x, int& y) const;
    // @0x134948: a tap that the job handling left over: a used-up decoration with PORTAL sends the
    // hero to it (true), one with MESSAGE shows it.
    bool OnClick();
    // @0x133c80: its "use" icon shows: usable, unlocked by its quests, and either INTERACT_ICON
    // (not "none") or its required item in the bag.
    bool ShowUseItemHover() const;
    // +0x44 @0x13577c, every frame: its animation; the end of the job on it (its uses, the quest
    // steps, the drops, what its expression does after an interaction); the hero stepping on its
    // quest or teleport tiles; its sprite's visibility.
    void Update(double dt);
    void Finish();                                   // PORT: Update's part for a decoration used up
    // @0x132bcc: the job starts now; its price is paid, each resource in a "-n name" popup.
    void WorkStarted();
    // @0x133058 (Map::AssignEntities): a job that ended while the game was closed gives its DROP
    // straight to the player.
    void UpdateOfflineState();
    // @0x135260: a particle entity `name` at each of its type's particle points (count: how many
    // times it plays, 0 forever; randomStart: its animation starts at a random frame).
    void CreateParticles(const char* name, unsigned count, bool randomStart);
    void RemoveParticles();                          // @0x133c44
    void UpdateDamageParticles();                    // @0x135550 smoke below 66% of its hit points, fire below 33%
    void ReceiveDamage(int damage);                  // @0x1356ec
    void TeleportPlayerTile(int x, int y);           // @0x1337d4 the hero's squad jumps to the tile
    bool TeleportAction(unsigned i);                 // @0x13391c PORTAL's i-th tile (after the tutorials)
    bool PortalAction();                             // @0x131fd8 the hero walks to its portal
    void TeleportPlayerMap(int map);                 // @0x1339cc
    // @0x1339f8: the hero stands on one of TELEPORT_TILE's tiles and the decoration is unlocked:
    // to its map, or to its tile.
    void HandleTeleportTile(const MetaData* d);
    void TrapDamage(const MetaData* d);              // PORT: Update's TRAP_DAMAGE part
    void TrapPattern();                              // PORT: Update's TRAP_PATTERN part
};

struct Patch {
    uint32_t infoId = 0;               // +0x00 its AreaInfo's id (the copy's x, y, w, h, a below)
    uint32_t areaId = 0;               // +0xe4
    bool owned = false;                // +0xe8
    bool bordered = false;             // +0xe9 (recomputed in UpdateAreaBorders)
    int x = 0, y = 0, w = 0, h = 0;    // +0x8c..+0x98 (from AreaInfo)
    int price = 0;                     // +0x9c (AreaInfo "c")
    bool buyable = false;              // +0xa8
    Render::Sprite* sign = nullptr;    // +0xec the for-sale sign (UpdateAreaBorders), tapped to buy
    std::vector<std::unique_ptr<Decor>> decors;      // +0xac
    std::vector<std::unique_ptr<Building>> buildings;
    std::string mask;                  // chunk 7 ('0'/'1' per tile, x-major)
    struct Tile { int x, y; bool blocked; };   // 0xc bytes
    std::vector<Tile> tiles;           // +0xcc a campaign map's tiles from the mask (CreateRoadAI)
};

// @0x1c3518 (with LoadPlayer @0x1c0cd8): a map from its save block (SaveManager::GetMapData);
// the header sets the current map id. lrand48 is seeded with time (the random decorations and
// the dark grass use GameState::playerSeed).
bool Load(SaveManager::SaveBlock* block, uint32_t time, bool newGame = false, bool fromCache = false);
// @0x1bc954: the patch made from area `id` (AreaInfo id), made now if there is none (null when the
// area does not exist).
Patch* GetAreaByID(uint32_t id);
// @0x1b7264: the area to buy next: of the areas for sale (not owned, bordering the player's land,
// buyable) the cheapest; its area id, 0 when there is none.
uint32_t GetNextAreaID();
// @0x1b8724: a finished building of type id is to stand at the tile; Map::Update places one of
// the queued buildings each time it runs.
void EnqueueBuildingPlacement(int x, int y, uint32_t id);
// @0x1bebc8: the decoration at the place `key` names (y + x * 0x100 + map * 0x10000; in the city
// also with map 0x2f), the nearest to the hero when several match; null without one or the hero.
Decor* GetDecorationToInteract(uint32_t key);
// @0x1b6f60: of the decorations the last such search collected, the nearest to the tile.
Decor* GetNearestFilteredDecoration(unsigned x, unsigned y);
Decor* GetDecorationThatIsReplacedWith(uint32_t id);   // @0x1ba9c4 the first whose REPLACE_OBJ is id
Decor* GetTriggeredDecoration(int x, int y);           // @0x1baa60 the first with a trigger tile there
Decor* GetBlockingDecoration(int x, int y);            // @0x1bab04 the first with a block tile there
void RemoveBlockedTiles();                             // @0x1c0708 the blocked tiles open for a path search
void RestoreBlockedTiles();                            // @0x1b8824
// The searches below collect the visible, still usable decorations that match and give the one
// nearest to the hero (null without the hero).
// @0x1be5e8: any (unlocked: not locked; reachable: the hero has a way to its action point).
Decor* GetDecorationWithAnyInteraction(bool unlocked, bool reachable);
Decor* GetDecorationWithDrop(uint32_t item);           // @0x1be870 CanDropItem(item)
Decor* GetDecorationWithAnyDrop();                     // @0x1bea14 unlocked, with a DROP
// @0x1bf1ec: key is the decimal digits of the map id followed by those of the decoration id (in
// the city also with map 47); one with a worker is preferred.
Decor* GetDecorationToClick(uint32_t key);
// @0x1bca94 (Override): a named value for object `id` (with sub-id) is kept in a table; a value
// already there for the same name is replaced too, and the entry is always added.
// UNVERIFIED (online, dormant): Map::LoadPlayer (0x1c0cd8) applies the table to the map's objects.
void ChangeObjectAttribute(uint32_t id, uint32_t sub, const char* name, const char* value);
// @0x1b6cb4: a workshop or farm (class 2 / 0xd) whose type delivers the contract list; with
// `done` one whose order is ready is preferred.
Building* GetBuildingWithContract(const Contracts::Contract* contract, bool done);
// @0x1bacd0: the first open building that can still be upgraded.
Building* GetUpgradeableBuilding();
// @0x1bf540: an open farm (of type id, 0: any) that still has a soil patch to buy.
Building* GetFarmWithPatchToBuy(uint32_t id);
// Every decoration's quest reactions. (Buildings have none: MapObject::OnTaskCompleted @0x1c7748,
// OnSubTaskCompleted @0x1c774c and OnInteract @0x1c7750 are empty.)
void PrepareDecorationsForSetup();          // @0x131268
void ExecuteStaticMeta();                   // @0x1baf70 OnSetup of each (Map::Load)
void OnTaskCompleted(uint32_t task);        // @0x1b65d4
void OnSubTaskCompleted(uint32_t subtask);  // @0x1b6650
void OnInteract(uint32_t object);           // @0x1b66cc
void AssignUniqueIDs();
void Free();
// @0x1bbbcc: the game state and the current map into the main save, then the save file (type 1:
// the online save, not ported). In the city the buildings' offline records are refreshed first.
void Save(int type);
bool SaveMap();                                        // @0x1bb930 false: a city without buildings
void SaveState();                                      // @0x1bbb80
void SafeSave();                                       // @0x1bbe90 (closes dialogs, collects drops)

int GetGridWidth();
int GetGridHeight();
uint32_t GetMapID();
int GetTileset();

Building* GetBuilding(int x, int y);                   // @0x1b63f0
Building* GetCurrentFarm();                            // @0x1b73c4 the farm being visited (0x6136b8)
// The farm view: a 3 x 4 grid of 196 x 98 px tiles at world (150, 2880), used while the current
// location is 1 (GetBlock/SetBlock then use its blocks).
int GetFarmGridWidth();                                // @0x1b6b50
int GetFarmGridHeight();                               // @0x1b6b64
float GetFarmWorldX();                                 // @0x1b739c
float GetFarmWorldY();                                 // @0x1b73b0
Patch* GetFarmPatch();                                 // @0x1b7388
void CreateFarmPatch();                                // @0x1b94f0
void ShowFarm(bool show, Building* b);                 // @0x1b9228
void SetBuilding(int x, int y, Building* b);           // @0x1b6388
// @0x1b6434: the tile's decoration, else its virtual decoration (a road being placed).
Decor* GetDecoration(int x, int y);
Decor* GetVirtualDecoration(int x, int y);             // @0x1b6484
void SetVirtualDecoration(int x, int y, Decor* d);     // @0x1b63cc
void SetDecoration(int x, int y, Decor* d);            // @0x1b63a8
// @0x1bafe8: the building at the tile leaves the grid (its footprint, the waypoints' weights back to
// 1). Unless onlyUnlink it is destroyed (OnDestroy), taken out of its patch and its hovers, and
// deleted when deleteIt.
void RemoveBuilding(int x, int y, bool onlyUnlink, bool deleteIt);
// @0x1bb148: the decoration at the tile leaves the grid; unless onlyUnlink its sprite goes and it
// is removed from its patch. PORT: kept in the patch list flagged removed (not saved), as
// RemoveDecorationAt does, so pointers held elsewhere stay valid.
void RemoveDecoration(int x, int y, bool onlyUnlink);
Decor* GetDecorationIgnoringBuildzones(int x, int y);  // @0x1c1a98 the one placed on exactly (x, y)
// @0x1c1828: the patch containing the tile; without one, an error and (unless quiet) a new owned
// patch covering the whole grid.
Patch* GetPatchForCoordinates(int x, int y, bool quiet);
// @0x1c19cc: every tile of a w x h zone starting at (x, y) lies on owned land.
bool IsValidAreaForBuildZone(int x, int y, int w, int h);
Building* GetTavern();                                 // @0x1b8d30
Building* GetBarracks();                               // @0x1b8dd4
Building* GetHQ();                                     // @0x1b6c94 the castle (99, else 100)
// @0x1c1b00 (city only): road tiles in the rectangle switch to the tile matching their neighbours.
void UpdateRoadConnections(int x0, int y0, int x1, int y1);
int GetRoadConnectionType(const Decor* d);             // @0x131350
bool GetBlock(int x, int y);                           // @0x1b8fd8
void SetBlock(int x, int y, bool block);               // @0x1b9078
void CreateRoadAI();                                   // @0x1bb868 the waypoint graph (AI.h)
// @0x1baba8: an owned building without worker and builder that needs one: a ready tree/rock with
// resources left, or one waiting for construction or an upgrade.
Building* GetIdleWorkplace();
Building* GetBuildingWithID(uint32_t id);              // @0x1b6c04 (BuildingData id)
void FillFarmBuildings();                              // @0x1be338 the opened farms, in map order (0x613818)
// The opened farm after / before b, wrapping around (BottomFarmWindow's arrows); the only farm if
// there is one; null if there is none or b is not an opened farm.
Building* GetFarmAfter(Building* b);                   // @0x1be4a4
Building* GetFarmBefore(Building* b);                  // @0x1be544
// @0x1ba4b8: a tap on a for-sale sign opens LandWindow for that area (in the player's city,
// after the tutorials). Screen coordinates.
bool ClickToBuyArea(int x, int y);
// @0x1bdb58: every patch of the area becomes owned and the borders are redrawn.
void BuyArea(uint32_t areaId);
// @0x1b6dd4: a building of that id being upgraded (upgrading) or waiting for its construction.
Building* GetUnfinishedBuildingWithID(uint32_t id, bool upgrading);
// @0x1b6ea0: a construction site or an upgrade that will add people (givePopulation).
Building* GetUnfinishedBuildingedWithPopulation();
// @0x1bad78: an opened building of that id below its last upgrade.
Building* GetUpgradeableBuildingWithID(uint32_t id);
int GetBuildingCount(uint32_t id, bool built);         // @0x1b7068 (built: no builder needed any more)
int GetBuildingMaxUpgrade(uint32_t id);                // @0x1b7118 1 + the highest level of that id, 0 if none
// @0x1ba618: the people that sites under construction (givePopulation) and upgrades in progress
// (their upgrade's givePopulation) will add.
int GetPendingWorkerCount();
// @0x1ba6dc: the people buildings employ: cost_population plus every passed upgrade's population,
// less the 4 free ones.
int GetUsedWorkerCount();
void AssignEntities();                                 // @0x1b99f0 (Map::Load)
void UpdateOfflineResources();                         // @0x1b8af0 (LoadSavedGame)
// @0x1b8e7c: the storage (class 7) nearest to b in tiles, on any patch; b itself is skipped when
// notSelf.
Building* GetNearestStorage(Building* b, bool notSelf);
void GetOwnedAreaBorders(int& minX, int& minY, int& maxX, int& maxY);   // @0x1b5fcc
// @0x1b8bfc: the storage limit is the sum of every storage's "storage_space" at its level.
void UpdateStorageMax();
void UpdateStorages();                     // @0x1b8b74 every storage's piles (Building::UpdateStorage)
// @0x1b7224: the city's storage limit; away from the city (map id != 0) the last non-zero one.
int GetTotalStorageLimit();
void WorldCoordinatesToScreen(int& x, int& y);         // @0x1b60fc
void MouseCoordinatesToWorld(int& x, int& y);          // @0x1b6004 screen pixels to world
void WorldCoordinatesToTile(int& x, int& y);           // @0x1b61cc
bool IsLoaded();                                       // gameLoaded 0x613798
// @0x1bb414: the building with an opaque sprite pixel at the world point (the lowest one: the
// nearest to the viewer). Building::Contains @0x11e444, IsLower @0x11c658.
Building* GetBuildingAtCoords(int x, int y);
// @0x1bb280: the decoration hit at the world point, preferring interactive ones (any = false
// skips plain decorations in the first pass).
Decor* GetDecorAtCoords(int x, int y, bool any);

void TileCoordinatesToWorld(int& x, int& y);           // @0x1b632c
void TileCoordinatesToLinear(int& x, int& y);          // @0x1b62f4
// @0x1c4dd8, the part ported so far: every building's Update, the clouds, the birds, the layer-8
// re-sort, the map timer and Background::Update.
void Update(double dt);
// @0x1bb4d4 (Map::Load, once): the birds' frames (images/bird/, three looping chains of four
// frames, 0.2 s each) and the cloud images (images/Effects/clouds/).
void PrepareImages();
void UpdateResources();                                // @0x1ba5ac every building's gathered pile
// @0x11c428 / @0x130f18: buildings / decorations may update their fog visibility this frame (set by
// Map::Update every 0.1 s; read on campaign maps only).
void AllowBuildingVisibilityUpdate(bool on);
void AllowDecorationVisibilityUpdate(bool on);
bool DecorationVisibilityUpdateAllowed();   // PORT: reads the flag (0x60ef21)
bool BuildingVisibilityUpdateAllowed();     // PORT: reads the flag (0x60ef20)
// @0x1b85c0 / @0x1b7a38: the first patch's decorations' uses and start times are remembered by
// tile (a battle map's reset), and given back to the decorations loaded next.
void StoreDecorationInfo();
void RestoreDecorationInfo();
void RemoveHoldOfEntities();                // @0x1b8300 every building and decoration forgets its entities
void UpdateSpawns();     // @0x1c2914 (Map::Load) the player's tile and the spawn points' entities
// ---- spawn points and portals (MapSpawn.cpp)
void LoadSpawnPoints(SaveManager::SaveBlock* block);   // @0x1bc0b0
void SaveSpawnPoints();                                // @0x1b9630
void LoadPortals(SaveManager::SaveBlock* block);       // @0x1bbeb8
void SavePortals();                                    // @0x1b955c
void LinkPortals();                                    // @0x1b846c
void FreeSpawnPointsAndPortals();                      // PORT: Map::Free's part
SpawnPoint* GetSpawnByIndex(unsigned i);               // @0x1b7300
unsigned GetSpawnPointCount();                         // PORT (0x6136b0)
SpawnPoint* GetSpawnPointAt(int x, int y);             // @0x1b7324
void StoreSpawnpointInfo();                            // @0x1b88b0
void RestoreSpawnpointInfo();                          // @0x1b7b08
Decor* GetPortalToMap(uint32_t map);                   // @0x1b6fe8 the decoration that leads there
// ---- the fog (Fog.h)
void AddFog(int x, int y, int size);                   // @0x1b8514
void ClearFog();                                       // @0x1b8808
void RemoveFog(float x0, float y0, float x1, float y1);   // @0x1b84d4
void ToggleFogRender();                                // @0x1b84d0
void StoreFogInfo();                                   // @0x1b88ac
void RestoreFogInfo();                                 // @0x1b88a8
void UpdateBattleMap();                                // @0x1c4cfc
// ---- the hero's place over map switches
void StorePlayerPosition();                            // @0x1b7874 the next load starts where it stands
void RestorePlayerPosition();                          // @0x1b73ec
// @0x1b7d00: the hero back to the map's start tile (Locations), else its own spawn point's.
void PlayerDefaultSpawn();
void PlayerNextSpawn(int x, int y);                    // @0x1b747c the next load's start tile
int GetLastPlayerX();                                  // @0x1b5f58
int GetLastPlayerY();                                  // @0x1b5f68
// @0x1b5f7c: Map::Update switches to the map (0x60eff8) on its next run.
void EnqueueLoadMap(int map, bool bossReward, bool save, float delay);
bool IsMapLoadQueued();                                // @0x1b5fb8
bool IsMapLoading();                                   // @0x1b68a4
void SpawnEntities();    // @0x1c0460 (Map::Load) the player's squad appears around it
// @0x1b6754: after the app's pause every building and decoration runs one Update with the time
// paused (main_Loop_Func).
void OfflineUpdate(double dt);
float GetSpriteZ(float a, float b, int c);             // @0x1b6814
// Extent (tiles) of the unowned patches that border owned land; the whole grid if there are none.
void GetAreaBorders(int& minX, int& minY, int& maxX, int& maxY);   // @0x1b6a20
void UpdateOwnedAreaBorders();                                     // @0x1b68f4
// ---- the scripted camera (MapCamera.cpp)
// @0x1c08cc: one more point for the view to visit: tile, seconds to stay and to travel, whether the
// hero may go on walking, a world point for the help arrow (0, 0: none), zoom on the way, whether
// the view returns to the hero after the last point, extra fog radius, a task to complete on leaving.
void AddCameraPoint(int x, int y, int wait, int travel, int keepHero, int arrowX, int arrowY, int f1c, float zoom,
                    bool returnToPlayer, float fogRadius, uint32_t task);
// PORT helper: the points of a CAM... expression (x:y:wait[:travel[:keepHero[:f1c[:zoom %[:fog
// radius[:task]]]]]]) from entry `first`, with the cinematic bars.
void AddCameraPath(const MetaData* list, unsigned first);
void ClearCameraPoints();                              // @0x1bf69c
void AddCameraShake(float time, float strength);       // @0x1b7438
void AddCameraCinematic();                             // @0x1b81a8
void UpdateCamera(float dt);                           // PORT: Map::Update's camera part
// @0x1bf6d4: a path that lets the hero walk is dropped (the hero was told to go somewhere).
void InterruptCamera();
bool IsCameraMoving();                                 // @0x1b82c4

}  // namespace Map
