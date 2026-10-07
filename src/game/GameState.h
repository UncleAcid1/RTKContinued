// GameState: the player's state (resources, level, settings, tutorial, ...). Being ported in
// milestone 3; the settings and areas are in game/Setting.h.
#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Map { struct Building; }
namespace SaveManager { struct SaveBlock; }
namespace Items { struct ItemInfo; }
class Entity;
class BaseCombat;

namespace GameState {

enum ResourceType {   // GameState::GetResourceName table @0x601568
    kLumber = 0, kRocks = 1, kFood = 2, kPlanks = 3, kStones = 4, kMeat = 5, kSausage = 6, kOil = 7,
    kGold = 8, kCrystal = 9, kExpAfter = 10, kLevel = 0xbb
};

extern bool updated;             // a change waiting to be saved
extern int resourceAmountMax;    // storage limit
extern int lastResourceAmountMax; // the last non-zero storage limit
extern int maxLevel;
extern int totalGoldSpent, totalGoldEarned, totalCrystalsSpent, totalXPEarned;
extern double totalTimeSpent;    // 0x613410 (Timer::AdvanceGlobalTime)

void Reset();
uint32_t GetResourceAmount(int type);          // @0x196e08
void SetResourceAmountValidated(int type, uint32_t v);   // @0x196ed0
uint32_t GetResourceAmountValidated(int type);   // @0x196c78 (same as GetResourceAmount)
int GetLevel();                                // @0x196e00 (resource 0xbb)
bool CheckStorageFull(int type);               // @0x196dd8
// @0x197018: add (or spend, negative) a resource; XP raises the level. Crystals only go down here.
void ChangeResourceAmount(int type, int amount);
void AddCrystals(int amount);                  // @0x196f68
int AdjustCrystalCost(int cost);               // @0x19a758 cost * Setting "crystal_mult"
const char* GetResourceName(int type);         // @0x190ce4
int StringToResourceType(const char* name);
int ExternalResourceTypeToInternal(unsigned type);   // @0x190d44 data files: 0 gold, 1 crystal, 2.. lumber..    // @0x192580 (case-insensitive; unknown -> GOLD)

bool IsPlayerCity();                           // @0x1905b8: city state 0 or 1
int GetCurrentLocation();                      // @0x190530: 0 city, 1 farm, 2 campaign, 3 arena
void SetCurrentLocation(int location);         // @0x190544
uint32_t GetCurrentMapID();                    // @0x190558
void SetCurrentMapID(uint32_t id);             // @0x19056c
bool IsTutorial();                             // @0x190ad4 tutorial < 0x18 (the opening campaign)
int GetPlayerWorkersCount();                   // @0x1927bc
int GetMaxWorkerCount();                       // @0x19a7a8
int GetGoblinCount();                          // @0x191a2c the storage goblins (entity class 0x10)
bool IsMalePlayer();                           // @0x190ca8
void SetPlayerGender(bool male);               // @0x190cbc
const char32_t* GetPlayerName();               // @0x190cd0
void SetPlayerName(const char32_t* name);      // @0x191e88
// The player's castle name; without one, StringTable "world_node_player_no_name".
const char32_t* GetCastleName();               // @0x191d98
void SetCastleName(const char32_t* name);      // @0x191e34
// The tutorial step (GameState::tutorial). HUD panels appear once it passes their thresholds;
// 0x100 is the finished city tutorial.
int TutorialStep();
int SecondTutorialStep();                      // GameState::secondTutorial
bool IsCityTutorial();                         // @0x190af4
int GetTutorialType();                         // @0x19067c (0x6129d4, saved in chunk 0x3d)
// GameState::currentCombat (milestone 4g creates combats; until then it is always null).
BaseCombat* GetActiveCombat();                 // @0x191050
inline bool IsCombatActive() { return GetActiveCombat() != nullptr; }   // @0x190f74
// @0x190f90: a combat whose type (vfunc +0x50) is above 2 is running. UNVERIFIED (milestone 4): no
// combats are ported (GameState::currentCombat is always null), so it is false.
inline bool IsBossCombatActive() { return false; }
bool IsTameTutorial();                         // @0x190bb4
// @0x190b8c a PvP tutorial fight is running. UNVERIFIED stand-in (milestone 4, PvP): false.
bool IsPvPTutorial();
bool TaskCompleted(unsigned id);               // @0x1908ac (the 0x1000-entry lookup cache is not ported)
bool IsTaskStarted(unsigned id);               // @0x190988
uint32_t GetTaskBeginTime(unsigned id);        // @0x198624 (0 when not begun)
uint32_t GetGameStartTime();                   // @0x19130c

const char* GetResourceIconName(int type);      // @0x190cfc the 16 px icons (table 0x601594)
const char* GetResourceWorkerIconName(int type);   // @0x190d2c (table 0x6015ec)
// @0x190d5c: the dropped crop's image for farm drop id (delivery id * 10 - 1 + order); wheat for
// the ids without their own.
const char* GetFarmDropImageName(unsigned id);
const char* GetResourceMapIconName(int type);   // @0x190d14 (table 0x6015c0)
const char32_t* GetResourceGameName(int type);  // @0x191cf8 StringTable name (table 0x601618)

// Delivery orders (0x612d50): resources a goblin carries from a building (a tree's or rock's pile)
// to a storage. Taken orders stay in the list (the removal functions run on building removal and
// game reloads, not ported yet).
struct Order {                 // 0x1c bytes
    bool taken = false;        // +0x00
    Map::Building* from = nullptr;   // +0x04
    Map::Building* to = nullptr;     // +0x08
    int type = 0;              // +0x0c resource type
    int amount = 0;            // +0x10
    int kind = 0;              // +0x14 0: pick up a pile (skipped while that resource is full)
    Entity* goblin = nullptr;  // +0x18
};
void PlaceOrder(Map::Building* from, Map::Building* to, int amount, int type, int kind);   // @0x19c3e0
// @0x196e0c: the newest order not taken (and not a pile pickup of a full resource), now taken.
Order* GetTopOrder();
int GetOrderCount(int type);                   // @0x190c20 orders of `type` a goblin carries
// @0x195ab4: drop up to n+1 orders from `from` (their goblins cancel work). Freed orders go back to
// the original's pool, so a goblin still holding one keeps valid memory; the port keeps them too.
void RemoveAllOrders(Map::Building* from, int n);
void RemoveAllTargetOrders(Map::Building* to, int n);   // @0x195a04 (at most n orders delivering to `to`)
void RemoveOrderOfWorker(Entity* goblin);       // @0x1953c8 the first order the goblin carries
void CancelWork(int type);                     // @0x191c8c goblins on `type` orders cancel work

// The pause counter (0x612d70): dialogs raise it while shown.
void RaiseGamePauseState();                    // @0x191064
void DropGamePauseState();                     // @0x191080 (never below 0)
bool IsPaused();                               // @0x191104

// ------------------------------------------------------------------------------ saved state
// Everything GameState::Save writes (GameStateSave.cpp). The containers of systems that are not
// ported yet (tasks, items, events, arena, soldiers, PvP, ...) are kept as the original holds
// them, so a save passes through the port unchanged.
extern int tutorial;            // 0x6134ac GameState::tutorial
extern int secondTutorial;      // 0x60efd0 GameState::secondTutorial
extern int lastSentStep;        // 0x6134b0 GameState::lastSentStep
extern uint32_t playerSeed;     // 0x613408 GameState::playerSeed (random decorations)
extern uint32_t mHPTS;          // 0x613454 GameState::mHPTS (hit point regeneration time)
extern uint32_t latestUniqueID; // the highest building unique id (Building::SetUniqueID)

struct PlayerItem {             // 0x18 bytes, the player's items (vector 0x612f90)
    uint32_t id = 0;            // +0x00
    Items::ItemInfo* info = nullptr;   // +0x04 Items::GetItemInfo
    uint32_t uniqueId = 0;      // +0x08
    uint32_t f0c = 0, f10 = 0;  // +0x0c +0x10
    bool f14 = false;           // +0x14
};
struct ItemBinding {            // 0xc bytes: the item bindings 0x612ad8 (10) and 0x612b50 (32)
    uint32_t id = 0;            // +0x00 an item id to bind after loading
    uint32_t uniqueId = 0;      // +0x04
    PlayerItem* item = nullptr; // +0x08
};
struct OfflineBuilding {        // BuildingInfoOffline, 0x20 bytes (vector 0x612d64)
    uint32_t id = 0, level = 0, contract = 0;   // +0x00 +0x04 +0x08
    uint32_t flags = 0;         // +0x0c the map id | 1 opened | 2 needs a builder | 4 upgrading
    uint32_t stateTime = 0;     // +0x10
    bool f14 = false, f15 = false;              // +0x14 +0x15
    uint32_t x = 0, y = 0;      // +0x18 +0x1c
};

void Save(uint32_t version);                   // @0x19d428 into the main save's current block
void Load(SaveManager::SaveBlock* block);      // @0x1a4934
void SaveEntities();                           // @0x192844
void LoadEntities(SaveManager::SaveBlock* block);   // @0x1930bc
// @0x19a11c: SoldierSlots' usable slots, 3 once the "squad_slot_02_quest_unlock" quest is done, else 2.
unsigned GetSoldierSlotCount();
Entity* GetFriendPlayer();                      // @0x1919ec the first class-0x16 entity (a visited friend)
void SetSoldierReserveSlots(unsigned n);  // @0x191d90 SoldierPool::SetFreeSlotsCount
unsigned GetSoldierReserveSlots();        // @0x191d94 SoldierPool::GetTotalSlotsFree
void AddOfflineBuilding(const OfflineBuilding& b);   // @0x194928
void ClearOfflineBuildings();                  // @0x19bda8
uint32_t GetBeltSlotCount();                   // @0x190788
uint32_t GetBeltItemAt(unsigned slot);         // @0x1907e8
int GetItemAmount(uint32_t id, bool belt);     // @0x19bdc8
void RemoveItem(uint32_t id, int count);       // @0x19510c
PlayerItem* GetItemByUniqueID(uint32_t uniqueId);   // @0x19623c
PlayerItem* GetFirstItemByID(uint32_t id, bool belt);   // @0x1962a4
PlayerItem* EnumItems(unsigned i);             // @0x196214 nullptr past the end
// The equipment slots: items.xml numbers them 1..10 (external), the bindings 0..9 (internal).
uint32_t ExternalItemBindingToInternal(uint32_t slot);   // @0x1906f0 (table 0x5809fc)
uint32_t InternalItemBindingToExternal(uint32_t slot);   // @0x190708 (table 0x580a24)

}  // namespace GameState
