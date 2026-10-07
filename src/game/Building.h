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
namespace GameData { struct BuildingData; struct BuildingPart; struct UpgradeInfo; }
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
    int lastContract = 0;        // +0x60 the last contract launched, 1-based
    double gatherAcc = 0.0;      // +0x68 gathering progress (seconds)
    int resources[11] = {};      // +0x70 resources held per type (storage: the player's amounts; farm: each soil patch's state)
    uint32_t patchArg[6] = {};   // +0x9c farm patch rot timer start (chunk 0x31)
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
    bool isCopy = false;         // +0x11c a Duplicate (the placement preview) until placed
    uint32_t uniqueId = 0;       // +0x120
    std::vector<Entity*> workers;  // +0x124 one slot per parking point
    std::vector<Entity*> livers; // +0x130 entities living here (AssignLiver)
    Entity* builder = nullptr;   // +0x13c
    Entity* farmer = nullptr;    // +0x140 the farm view's farmer (AIFarmerBig, 3f)
    std::vector<Entity*> patchEntities;   // +0x144 one soil patch entity (AIPatch) each (3f)
    int patchContract[6] = {};   // +0x150 farm patch contracts, 1-based (0 empty)
    Entity* farmEntity = nullptr;  // +0x15c
    int farmEntityId = 0;        // +0x160
    Entity* piles[4] = {};       // +0x164 storage piles (lumber, rocks, food, planks)
    Entity* trainee = nullptr;   // +0x170
    Render::Sprite* ring = nullptr;  // +0x174 ring under trees/rocks
    BuildingAnim anim;           // +0x178
    bool firstUpdate = true;     // +0x17c
    float patchProgress[6] = {}; // +0x180 farm view: each patch's current growth stage or work, 0..1
    bool liverAway = false;      // +0x199 a liver left for work (WorkerLeftToWork brings one back)
    int hp = 0, maxHp = 0;       // +0x1a8, +0x1ac (BuildingData hp)
    Entity* target = nullptr;    // +0x1b4
    int busy = 0;                // +0x1b8

    // vtable
    void FindBaseCoordinates();  // +0x08 @0x11cbb8
    void UpdateImage();          // +0x0c @0x129bf8
    void GetStartTile(int& tx, int& ty) const;   // +0x18 MapObject @0x1c7658
    void GetBuildZone(int& w, int& h) const;     // +0x1c MapObject @0x1c76e4
    // +0x20 @0x11e5f0: every footprint tile is free (fake decorations count as free when
    // ignoreFake), on the grid, on owned land and without an NPC.
    bool CanBePlaced(bool ignoreFake) const;
    bool IsMirrored() const { return mirrored; } // +0x24 MapObject @0x1c771c
    void ToggleMirror() { mirrored = !mirrored; }   // +0x28 MapObject @0x1c7724
    void Update(double dt);      // +0x44 @0x1267b0

    // @0x128070: a copy (operator= @0x127c24) without sprites, with its own animation controller,
    // marked isCopy; its image is built when updateImage.
    Building* Duplicate(bool updateImage) const;
    void LinkBaseToBuilding();   // @0x11e4a4
    bool Contains(int x, int y) const;           // @0x11e444 a pixel of one of its sprites at the world point
    // @0x11c900: the people it adds (givePopulation and its upgrades') less those it employs.
    int GetEffectOnPopulation() const;
    void PrepareToAction();      // @0x12035c hides its residents/piles while it is moved or removed
    void UndoAction();           // @0x12029c shows them again
    void OnMoved();              // @0x120454 residents and piles follow the new position
    void OnDestroy();            // @0x125b00 workers, orders and residents leave
    void CleanUp();              // @0x11d6a4 its own entities go
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
    // @0x11de78: a worker finished a step. On the visited farm, soil patch `patch` moves on: an
    // empty one is planted, a rotten one cleared, a ready one harvested (food and XP drop, and the
    // order is planted again if the gold is there), a dirty one is clean.
    void WorkEnded(int patch = -1);
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
    int GetMissionID() const;                    // @0x11c52c
    bool HasActiveContract() const;              // @0x1201d0 (farms: any patch)
    int GetResidentCount() const { return (int)livers.size(); }   // @0x123d70
    int GetFullGoldAmount() const;               // @0x123d84 5 per liver + collectmoney
    int GetReadyGoldAmount() const;              // @0x123da8 class 0/9: the share of a full collect time
    void CollectResources(bool& full, int& amount);   // @0x11dcac
    void OnStorageFull(int type);                // @0x11d43c
    // @0x11eb40: start order `index` (0-based) of the delivery list, on farm patch `patch` (-1:
    // the building's own order).
    void LaunchContract(unsigned index, int patch);
    const GameData::UpgradeInfo& GetNextUpgradeInfo() const;   // @0x11c6e4 upgrades[level]
    int GetContractSpeedUpCost() const;          // @0x11d020
    const char32_t* GetContractName() const;     // @0x11de3c
    void AddDeliveryOrder();                     // @0x11dc14 a storage delivers the order's price resource
    void UpdateGrowing();                        // @0x120a70
    int GetFirstGrowingPatchNum() const;         // @0x11f448
    int GetFarmState(int patch);                 // @0x11d7b4
    bool IsSoilPatchDirty(unsigned i) const;     // @0x11d1f0
    bool IsSoilPatchRotten(unsigned i) const;    // @0x11d224
    bool IsSoilPatchReady(unsigned i) const;     // @0x11d258
    bool IsSoilPatchActive(unsigned i) const;    // @0x11d28c growing
    bool IsSoilPatchEmpty(unsigned i) const;     // @0x11d2f0
    bool IsFarmSpeedUp(int i);                   // @0x11d324
    int GetFirstDirtySoilPatch() const;          // @0x11fe84
    int GetFirstRottenSoilPatch() const;         // @0x11fef0
    int GetFirstEmptySoilPatch() const;          // @0x11ff5c
    int GetFirstReadySoilPath();                 // @0x11ffc8 (sic)
    int GetFirstActiveSoilPatch() const;         // @0x120058
    int GetNextPatchToBuy() const;               // @0x11fe34
    void OnContractCompleted(bool silent);       // @0x11db64
    void OnBuilded();                            // @0x1260d4 (houses get workers, farms a farmer)
    void OnUpgraded();                           // @0x124df0
    void UpdateStorage();                        // @0x1244e4 the storage's resource piles
    void UpdateResources();                      // @0x123f50 a tree's or rock's gathered pile
    // @0x124308: the crystal speed-up. A tree/rock hands its "speedupresources" amount to goblins at
    // once (cut down when that empties it); a workshop's order ends now; otherwise the construction
    // or upgrade has no work left.
    void SpeedupBuilding();
    // @0x127170: start upgrading to `level` (paying the next upgrade's cost): closed while a
    // builder works off its upgrade time.
    void Upgrade(unsigned level);
    void HireGolbin();                           // @0x124d74 a delivery goblin from a storage
    void GetDeliveryTile(int& tx, int& ty) const;   // @0x11c5f8
    void SetupSmallFarm();                       // @0x11f498
    // Farm view (3f). SpawnFarm @0x128468: the soil patch entities (by farm type) with their saved
    // states and orders, and the farmer, at a patch still growing if there is one.
    void SpawnFarm();
    void DespawnFarm();                          // @0x120664
    // @0x11fcac: from patch `from` on: owned patches (up to resourceLeft) empty, the next one for
    // sale, the rest locked; resources[] keeps the states.
    void SetFarmPatches(int from);
    void OnSoilPatchBuy();                       // @0x11fdd8
    void CleanFarm(unsigned patch);              // @0x11d1a0
    void SpeedupFarm(int seconds, int patch);    // @0x11d344
    void RestoreFarm(unsigned patch);            // @0x11d444
    void UpdateOfflineStateNoWorker();           // @0x11ea90
    void UpdateOfflineState();                   // @0x126498 (a worker's workplace on load)
    void UpdateOfflineResources();               // @0x124450
};

// Map::FilterBuildingParts @0x129838: parts of `type` at `stage` (parts with stage -1 count in file
// order). The result is the shared list 0x6117c4, which FindBaseCoordinates also reads.
const std::vector<const GameData::BuildingPart*>& FilterBuildingParts(const GameData::BuildingData& d, int type, int stage);

}  // namespace Map
