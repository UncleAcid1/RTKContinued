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
// @0x190fd0: an arena combat (type above 4) is running.
bool IsArenaCombatActive();
// The loot collected per map (FoundItemsWindow lists it when a map is left or finished).
void AddMapItemCollectionInfo(uint32_t map, uint32_t id, int amount);
void AddMapResourceCollectionInfo(uint32_t map, int type, int amount);
int GetMapItemCollectionInfo(uint32_t map, uint32_t id);
int GetMapResourceCollectionInfo(uint32_t map, int type);
void ResetMapCollectionInfo(uint32_t map);
void SetMapStartTime(uint32_t map, uint32_t t);   // @0x1984b8
uint32_t GetMapStartTime(uint32_t map);           // @0x198484
void SetMapVisitTime(uint32_t map, uint32_t t);   // @0x1983d8
// The game speed factor (0x60efc0, 1 at start): Set @0x191138 ignores values above 1, Get @0x191160,
// Reset @0x191174 puts back 1.
void SetTimeModifier(float f);
float GetTimeModifier();
void ResetTimeModifier();
bool IsTameTutorial();                         // @0x190bb4
bool IsFirstVirtualTutorial();                 // @0x190b44 second-tutorial steps 0x200..0x205
// @0x190b8c a PvP tutorial fight is running. UNVERIFIED stand-in (milestone 4, PvP): false.
bool IsPvPTutorial();
bool TaskCompleted(unsigned id);               // @0x1908ac (the 0x1000-entry lookup cache is not ported)
bool IsTaskStarted(unsigned id);               // @0x190988
uint32_t GetTaskBeginTime(unsigned id);        // @0x198624 (0 when not begun)
void SetTaskBeginTime(unsigned id, uint32_t t);   // @0x1985e8
uint32_t GetTaskEndTime(unsigned id);          // @0x198530 (0 when not ended)
void SetTaskEndTime(unsigned id, uint32_t t);  // @0x1984f4
uint32_t GetTaskShowTime(unsigned id);         // @0x198718 when the task was first shown (0: never)
void SetTaskShowTime(unsigned id, uint32_t t); // @0x1986dc
// A step's counter (how often its action was done). GetSubTaskDone creates the counter, as the
// original's map lookup does.
int GetSubTaskDone(unsigned subtask);          // @0x19963c
void SetSubTaskDone(unsigned subtask, int n);  // @0x199600
void AddSubTask(unsigned subtask, int n);      // @0x199774
// @0x193978: for a task not completed yet a pop-up names it ("HIRE_TROOPS_TASK_LOCK") and true
// is returned: the caller's action waits for that task.
bool TryToHelpWithTask(unsigned task);
inline Entity* AddEntity(Entity* e) { return e; }   // @0x190c14 (empty in 5.11)
bool SubTaskCompleted(unsigned task, unsigned subtask);   // @0x199670 the task's step reached its amount
uint32_t GetCustomSubTaskRequrement(unsigned subtask);    // @0x198984 (0: none)
void SetCustomSubTaskRequrement(unsigned subtask, uint32_t v);   // @0x198948
// @0x19c6fc: the task joins the completed ones and gets its end time. With setBegin a task with a
// timeout also gets a begin time an hour before the timeout ran out.
void CompleteTask(unsigned id, bool setBegin);
void StartTask(unsigned id);                   // @0x19cd64 started; the entities are told
void ResetTask(unsigned id);                   // @0x19ce58 every record of the task and its steps goes
bool IsFeaturedTaskClicked(unsigned id);       // @0x1987d0
uint32_t GetEventStartTime(unsigned event);    // @0x198054 (0: never)
void SetEventStartTime(unsigned event, uint32_t t);   // @0x198018
uint32_t GetEventDifficulty(unsigned event);   // @0x197f74
// @0x1963bc / @0x1a9548: the tasks to complete / reset automatically (0 empties the list).
void AddAutocompleteTask(unsigned id);
void AddAutoresetTask(int id);
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
void ResetGamePauseState();                    // @0x1910a8
inline void Clean() {}                         // @0x190c10 (empty in 5.11)

// The server's switches (GameState::ServerSettings 0x89..0xb6, 0x612da8 set flags / 0x612e60
// values): the server names them "TSS_..." (mServerSettingsMap). Offline only the defaults exist.
bool IsServerSettingSet(int id);               // @0x19133c
int GetServerSetting(int id);                  // @0x19136c (0 when not set)
void SetServerSetting(int id, int value);      // @0x1913a0 (0x9d also sets maxLevel)
int GetServerSettingID(const std::string& name);   // the "TSS_..." name's id in mServerSettingsMap (0: none)
// @0x1aa0e4 (main_Loop_Init, first): unset switches read -1, the defaults, the names.
void InitDefaultSettings();

// @0x1964ec (the game's Update, each frame): a save the platform asked for goes to the server.
// Only the online paths set saveOnline (the Java/iOS mcAskToSaveOnline, CMyServer::
// NET_SendOnlineSave), so it stays 0 offline; the Facebook sync reminder is online too.
extern int saveOnline;                         // GameState::saveOnline
void Update(float dt);

// ------------------------------------------------------------------------------ saved state
// Everything GameState::Save writes (GameStateSave.cpp). The containers of systems that are not
// ported yet (tasks, items, events, arena, soldiers, PvP, ...) are kept as the original holds
// them, so a save passes through the port unchanged.
extern int tutorial;            // 0x6134ac GameState::tutorial
extern int secondTutorial;      // 0x60efd0 GameState::secondTutorial
extern int lastSentStep;        // 0x6134b0 GameState::lastSentStep
extern uint32_t playerSeed;     // 0x613408 GameState::playerSeed (random decorations)
extern uint32_t mHPTS;          // 0x613454 GameState::mHPTS (hit point regeneration time)
extern float mNextHP;           // GameState::mNextHP
extern uint32_t latestUniqueID; // the highest building unique id (Building::SetUniqueID)

struct PlayerItem {             // 0x18 bytes, the player's items (vector 0x612f90)
    uint32_t id = 0;            // +0x00
    Items::ItemInfo* info = nullptr;   // +0x04 Items::GetItemInfo
    uint32_t uniqueId = 0;      // +0x08
    uint32_t f0c = 0, f10 = 0;  // +0x0c the durability left, +0x10
    // @0x1ad2c4 Items::ItemData::IsBroken: an item with durability and none left.
    bool IsBroken() const;
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
void SetBeltSlotCount(uint32_t n);             // @0x190768 (at most 3)
void SetBeltSize(uint32_t n);                  // @0x190798
uint32_t GetBeltSize();                        // @0x1907ac
void SetBeltItemMode(bool on);                 // @0x1907c0
bool GetBeltItemMode();                        // @0x1907d4
void BindBeltItemTo(unsigned slot, uint32_t id);   // @0x190800 (its activation time back to 0)
void SetBeltItemActivationTimeAt(unsigned slot, uint32_t t);   // @0x190820
uint32_t GetBeltItemActivationTimeAt(unsigned slot);          // @0x190838
void ClearBeltItems();                         // @0x19639c the belt's unique ids (0x6133d8)
void AddBeltItem(uint32_t uniqueId);           // @0x1a3340
// The equipment (GameState::ItemBinding 0..9, internal numbering) and the customisations
// (GameState::CustomizationBinding); the original overloads GetItemAt / BindItemTo by binding type.
PlayerItem* GetItemAt(unsigned binding);       // @0x1906a8
PlayerItem* GetCustomizationAt(unsigned binding);   // @0x190720
void BindItemTo(unsigned binding, PlayerItem* item);           // @0x1906c4
void BindCustomizationTo(unsigned binding, PlayerItem* item);  // @0x19073c
bool IsItemBinded(const PlayerItem* item);     // @0x190850 an equipment item in one of the 10 bindings
// @0x19ec84: count new records (unless timesOnly), each with a new unique id and the item's
// durability; notes the first and the latest acquisition time. Returns the last record.
PlayerItem* AddItem(uint32_t id, int count, bool flag14, bool timesOnly, bool noShare);
void RemoveUniqueItem(uint32_t uniqueId);      // @0x195058 (the last record takes its place)
uint32_t GetItemBuffRemainingTime(uint32_t id);   // @0x198a3c seconds, 0 when over
void ActivateItemBuff(uint32_t id, uint32_t seconds);   // @0x198a90
uint32_t GetItemRecentTime(uint32_t id);       // @0x198db4
uint32_t GetItemAcquirementTime(uint32_t id);  // @0x198e6c
void SetupPlayerRegenerationState();           // @0x1921b8
void UpdatePlayerRegenerationState();          // @0x191d14
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
