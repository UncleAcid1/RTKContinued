// Map::Building: a building, tree or rock on the map, with its construction/upgrade, production,
// gathering and farm state. Port of Map::Building (libkingdom.so 5.11, 0x1d0 bytes, vtable 0x608100;
// constructor @0x127498). Offsets are the original's.
//
// State at a glance (buildingClass = BuildingData+0x1ac):
//   construction: built (+0xe4) == 0, opened (+0xe0) == 0, buildLeft (+0x48) counts down while the
//                 builder works; at 0: built = 1, OnBuilded. An upgrade is the same with upgrading
//                 (+0xe8) = 1 and ends with level (+0x50) + 1, OnUpgraded.
//   class 4 (trees, rocks): resourceState (+0x104) 0 regrowing through BuildingData.respawn stages from
//                 growStart (+0x108), 1 ready, 2 cut; resourceLeft (+0x100) is what a worker can gather.
//   class 2/0xc (workshops, training): contract (+0x58, 1-based) started at stateTime (+0x40).
//   class 0xd (farms): six soil patches, patchContract (+0x150) / patchStart (+0xb4).
// Workers, builders and the farm/storage entities are goblin Entities (milestone 3c); until they
// exist those pointers stay null and the code that spawns them is marked UNVERIFIED (3c).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Render { struct Sprite; struct Texture; }
namespace GameData { struct BuildingData; struct BuildingPart; }
class MetaData;
class Entity;

namespace Map {

struct Patch;
struct Building;

// BaseAnimController<Map::Building> (0x1c bytes, +0x178): cycles a part texture's frames.
struct BuildingAnim {
    int frame = 0;               // +0x04
    int shownFrame = -1;         // +0x08
    Render::Texture* tex = nullptr;  // +0x0c
    Building* owner = nullptr;   // +0x10
    float acc = 0.f;             // +0x14
    bool paused = false;         // +0x18 (a cut tree's last stage)
    void Update(float dt);       // @0x11e76c
};

struct Building {
    Building();                  // @0x127498
    ~Building();                 // @0x1279cc

    // MapObject part
    Patch* patch = nullptr;      // +0x04
    int index = 0;               // +0x08 slot in the patch's list
    uint8_t x = 0, y = 0;        // +0x0c, +0x0d tile
    int linearX = 0, linearY = 0;  // +0x10, +0x14 (TileCoordinatesToLinear of x, y)
    uint32_t id = 0;             // +0x18
    bool mirrored = false;       // +0x1c
    MetaData* meta = nullptr;    // +0x20 MetaExpression (quest buildings)  UNVERIFIED: kept as text
    std::string metaText;
    float baseX = 0, minX = 0, maxX = 0, baseY = 0, maxY = 0, minY = 0;  // +0x24 +0x28 +0x2c +0x30 +0x34 +0x38
    Render::Sprite* mainSprite = nullptr;  // +0x3c the last part sprite created

    uint32_t stateTime = 0;      // +0x40 when the current timer started (global time)
    double buildLeft = 0.0;      // +0x48 seconds of construction/upgrade work left
    int level = 0;               // +0x50 upgrade level / growth stage
    uint32_t f54 = 0;            // +0x54
    int contract = 0;            // +0x58 active contract, 1-based
    bool contractDone = false;   // +0x5c
    double gatherAcc = 0.0;      // +0x68 gathering progress (seconds)
    int resources[11] = {};      // +0x70 resources held per type (storage: the player's amounts)
    uint32_t patchArg[6] = {};   // +0x9c farm patch data (chunk 0x31)
    uint32_t patchStart[6] = {}; // +0xb4 farm patch start times (chunk 0x2d)
    const GameData::BuildingData* data = nullptr;  // +0xcc
    float partZ = 0.f;           // +0xd0 z step per part sprite
    std::vector<Render::Sprite*> sprites;  // +0xd4 sprite chain (newest first on the original)
    bool fd8 = false;            // +0xd8
    bool flashing = true;        // +0xd9 with flashTime: the just-opened highlight
    float flashTime = 0.f;       // +0xdc
    bool opened = false;         // +0xe0
    int built = 0;               // +0xe4
    int upgrading = 0;           // +0xe8
    int fec = 0, ff0 = 0;        // +0xec, +0xf0
    int needsBuilder = 0;        // +0xf4
    uint32_t ff8 = 0;            // +0xf8
    uint32_t lastGather = 0;     // +0xfc
    int resourceLeft = 0;        // +0x100
    int resourceState = 0;       // +0x104 class 4: 0 growing, 1 ready, 2 cut
    uint32_t growStart = 0;      // +0x108
    int f10c = 0;                // +0x10c
    uint32_t f110 = 0, f114 = 0; // +0x110, +0x114
    std::string resourceText;    // +0x118 "TYPE=n|..." (handed to decorations)
    uint32_t uniqueId = 0;       // +0x120
    std::vector<Entity*> workers;  // +0x124 one slot per parking point
    std::vector<Entity*> livers; // +0x130 entities living here (AssignLiver)
    Entity* builder = nullptr;   // +0x13c
    int patchContract[6] = {};   // +0x150 farm patch contracts, 1-based (0 empty)
    Entity* farmEntity = nullptr;  // +0x15c
    int farmEntityId = 0;        // +0x160
    Entity* piles[4] = {};       // +0x164 storage piles (lumber, rocks, food, planks)
    Entity* trainee = nullptr;   // +0x170
    Render::Sprite* ring = nullptr;  // +0x174 ring under trees/rocks
    BuildingAnim anim;           // +0x178
    bool firstUpdate = true;     // +0x17c
    bool liverAway = false;      // +0x199 a liver left for work (WorkerLeftToWork brings one back)
    int hp = 0, maxHp = 0;       // +0x1a8, +0x1ac (BuildingData hp)
    Entity* target = nullptr;    // +0x1b4
    int busy = 0;                // +0x1b8

    // vtable
    void FindBaseCoordinates();  // +0x08 @0x11cbb8
    void UpdateImage();          // +0x0c @0x129bf8
    void GetStartTile(int& tx, int& ty) const;   // +0x18 MapObject @0x1c7658
    void GetBuildZone(int& w, int& h) const;     // +0x1c MapObject @0x1c76e4
    bool IsMirrored() const { return mirrored; } // +0x24 MapObject @0x1c771c
    void Update(double dt);      // +0x44 @0x1267b0

    void LinkBaseToBuilding();   // @0x11e4a4
    bool IsOpened() const;       // @0x11c6a8 (class 4 always)
    void SetOpened() { opened = true; }          // @0x11c6c0
    void SetClosed(bool c) { if (c) opened = false; }  // @0x11c6cc
    void SetUniqueID(uint32_t uid);              // @0x11c9d4
    bool BuilderAssigned() const { return builder != nullptr; }   // @0x11c8f0
    bool BuilderIsWorking() const;               // @0x11d394
    void AssignWorker(Entity* e, int slot);      // @0x126054 (the builder while not opened)
    void AssignLiver(Entity* e);                 // @0x124d18
    void WorkerLeftToWork();                     // @0x11e3d4
    void WorkStarted();                          // @0x11e324
    void WorkEnded();                            // @0x11de78
    void GetSpawnTile(int& tx, int& ty) const;   // @0x11c538
    void GetWorkTile(int& tx, int& ty) const;    // @0x11c568
    void GetBuildTile(int& tx, int& ty) const;   // @0x11c598
    void GetBuildingSpot(float& wx, float& wy) const;   // @0x11c75c
    void GetParkingSpot(unsigned i, float& wx, float& wy) const;   // @0x11c7e4 (1-based)
    bool WorkerAssigned(int i) const;            // @0x123e48
    bool WorkerIsWorking(int i) const;           // @0x11d36c
    void RemoveWorker(Entity* e);                // @0x11d8a0
    int GetContractTime(int patch) const;        // @0x11c44c
    float GetContractProgress(int offset, int patch) const;   // @0x11d570
    int GetGatheredResCount() const;             // @0x11c9c0
    bool IsBusy() const { return busy != 0; }    // @0x11cb2c
    int GetReadyTime() const;                    // @0x11d4a0
    void ResetResource();                        // @0x11d530
    void UpdateGrowing();                        // @0x120a70
    int GetFirstGrowingPatchNum() const;         // @0x11f448
    int GetFarmState(int patch);                 // @0x11d7b4
    void OnContractCompleted(bool silent);       // @0x11db64
    void UpdateStorage();                        // @0x1244e4
    void SetupSmallFarm();                       // @0x11f498
    void UpdateOfflineStateNoWorker();           // @0x11ea90
};

// Map::FilterBuildingParts @0x129838: parts of `type` at `stage` (parts with stage -1 count in file
// order). The result is the shared list 0x6117c4, which FindBaseCoordinates also reads.
const std::vector<const GameData::BuildingPart*>& FilterBuildingParts(const GameData::BuildingData& d, int type, int stage);

}  // namespace Map
