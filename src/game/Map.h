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

namespace Render { struct Sprite; }
namespace GameData { struct DecorData; }
namespace SaveManager { struct SaveBlock; }

namespace Map {

struct Patch;

struct Decor {
    Patch* patch = nullptr;            // +0x04
    uint8_t x = 0, y = 0;              // +0x0c, +0x0d
    uint32_t id = 0;                   // +0x18
    bool mirrored = false;             // +0x1c (map flag bit 0)
    bool visible = true;               // +0x1d
    std::string metaText;              // +0x20 MetaExpression (quests)  UNVERIFIED: kept as text
    mutable const GameData::DecorData* data = nullptr;  // +0x3c (GetData fills it on first use)
    bool fake = false;                 // +0x43 random decoration (Patch::AddRandomDecors)
    // +0x44 (map flag bit 1; also set on unowned city patches and in the tame tutorial). Decor::IsFake
    // is +0x43 || +0x44. UNVERIFIED: only loaded and saved so far.
    bool hidden = false;
    uint32_t collectStart = 0;         // +0x48 when its taxes (DecorData collectTime) started
    uint32_t f4c = 0;                  // +0x4c (saved byte)
    uint32_t f50 = 0, f54 = 0;         // +0x50 +0x54
    std::string resourceText;          // +0x58 "TYPE=n|..." (parsed into +0x5c per resource)
    Render::Sprite* sprite = nullptr;  // +0x88
    bool removed = false;

    bool IsFake() const { return fake || hidden; }   // @0x130e20
    // @0x131234: the data of `id`, looked up on first use. UNVERIFIED: an id that is a building's
    // (the original then looks it up and drops it) gives null.
    const GameData::DecorData* GetData() const;
    void GetStartTile(int& tx, int& ty) const;       // +0x18 MapObject @0x1c7658
    void GetBuildZone(int& w, int& h) const;         // +0x1c MapObject @0x1c76e4
    void ToggleMirror() { mirrored = !mirrored; }    // +0x28 MapObject @0x1c7724
    // +0x20 @0x134554: every tile of the footprint is free (a fake decoration counts as free when
    // ignoreFake; the tile's virtual decoration never blocks), on the grid and not on unowned land.
    bool CanBePlaced(bool ignoreFake) const;
    void UpdateImage();                              // +0x0c @0x134c14
    void UpdateMapLink();                            // @0x1346b8
    // +0x2c @0x1344b0: the sprite has a pixel at the world point; a plain decoration (no quest, portal,
    // worker or collect time) only counts when `any`.
    bool Contains(int x, int y, bool any) const;
    void ReplaceWith(uint32_t id);                   // @0x131f08
    // @0x131284: the crystal speed-up of a decoration's job: collectStart = now + ~f50.
    void SpeedupDecoration();
};

struct Patch {
    uint32_t areaId = 0;               // +0xe4
    bool owned = false;                // +0xe8
    bool bordered = false;             // +0xe9 (recomputed in UpdateAreaBorders)
    int x = 0, y = 0, w = 0, h = 0;    // +0x8c..+0x98 (from AreaInfo)
    bool buyable = false;              // +0xa8
    Render::Sprite* sign = nullptr;    // +0xec the for-sale sign (UpdateAreaBorders), tapped to buy
    std::vector<std::unique_ptr<Decor>> decors;      // +0xac
    std::vector<std::unique_ptr<Building>> buildings;
    std::string mask;                  // chunk 7 ('0'/'1' per tile, x-major)
};

// @0x1c3518 (with LoadPlayer @0x1c0cd8): a map from its save block (SaveManager::GetMapData);
// the header sets the current map id. lrand48 is seeded with time (the random decorations and
// the dark grass use GameState::playerSeed).
bool Load(SaveManager::SaveBlock* block, uint32_t time);
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
// @0x1c4dd8, the part ported so far: every building's and decoration's Update.
void Update(double dt);
float GetSpriteZ(float a, float b, int c);             // @0x1b6814
// Extent (tiles) of the unowned patches that border owned land; the whole grid if there are none.
void GetAreaBorders(int& minX, int& minY, int& maxX, int& maxY);   // @0x1b6a20
// Camera paths (tutorial and quest camera moves, Map::AddCameraPoint) are not ported yet, so the
// list is always empty: InterruptCamera does nothing and IsCameraMoving is false.
void InterruptCamera();                                // @0x1bf6d4
bool IsCameraMoving();                                 // @0x1b82c4

}  // namespace Map
