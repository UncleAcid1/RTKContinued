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
class BaseSquad;

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
// @0x1938f4: the first storage goblin (class 0x10) at home (map 0) that is not fading in or out;
// its work is cancelled.
Entity* GetGoblin();
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
void SetTutorialType(unsigned type);           // @0x190668
// The running fight (GameState::currentCombat, Combat.h). It is updated by GameState::Update and
// deleted by EndActiveCombat.
BaseCombat* GetActiveCombat();                 // @0x191050
inline bool IsCombatActive() { return GetActiveCombat() != nullptr; }   // @0x190f74
void SetCombat(BaseCombat* c);                 // @0x191ee0 (the PvP arena: no HP over-limit)
// @0x191f10: the fight is deleted (end: BaseCombat::EndCombat first); the HP over-limit may be used again.
void EndActiveCombat(bool end);
bool IsBossCombatActive();                     // @0x190f90 its type is above 2 (boss, arena)
bool IsArenaCombatActive();                    // @0x190fd0 above 4
bool IsPvPCombatActive();                      // @0x191010 above 5
bool IsVirtualTutorial();                      // @0x190b68 second-tutorial steps 0x200..0x21b
extern int mBossSpawnID;                       // GameState::mBossSpawnID (boss fights: milestone 5)
extern uint32_t mReturnMapID;                  // GameState::mReturnMapID the map a boss / arena fight was entered from
// The fight a map load sets up (Map::SpawnEntities) and where it stands (BaseCombat::SwitchTo*):
extern std::vector<uint32_t> mBossWaves;       // GameState::mBossWaves the enemies' entity ids, the boss last
extern std::vector<int> mBossWavesHP;          // GameState::mBossWavesHP their HP (0: their own)
extern int mLastBossXY[2];                     // GameState::mLastBossXY
extern int mLastBossPlayerXY[2];               // GameState::mLastBossPlayerXY
extern int mLastPlayerDir;                     // GameState::mLastPlayerDir
extern int mCurrentBossCombatType;             // GameState::mCurrentBossCombatType 3 boss, 4 waves, 5 arena, 6 PvP arena
// GameState::mBossMapPositions (tile pairs, from the map's "bat" list): [2,3] the player's
// spawn, [6,7] the enemies', [8,9] the arena's centre (the camera), [10,11] its size.
extern int mBossMapPositions[12];
// The hero's HP comes back by itself, one at a time (mNextHP counts down to the next).
void UpdateHPRegen(float dt);                  // @0x19203c
int GetTimeTillNextHP();                       // @0x190f14 seconds
int GetVirtualHP();                            // @0x190f30 HP regenerated while he lay dead in a fight
void ResetVirtualHP();                         // @0x190f44
// PORT (freemium removal, user decision 2026-10-13): seconds per regenerated HP. A full bar takes
// at most 4 minutes (the original: Entity::GetHPRegeneration seconds per point, 3 to 5 minutes each).
float HpRegenSeconds(const Entity* player);
// The loot collected per map (FoundItemsWindow lists it when a map is left or finished).
void AddMapItemCollectionInfo(uint32_t map, uint32_t id, int amount);
void AddMapResourceCollectionInfo(uint32_t map, int type, int amount);
int GetMapItemCollectionInfo(uint32_t map, uint32_t id);
int GetMapResourceCollectionInfo(uint32_t map, int type);
void ResetMapCollectionInfo(uint32_t map);
void SetMapStartTime(uint32_t map, uint32_t t);   // @0x1984b8
uint32_t GetMapStartTime(uint32_t map);           // @0x198484
void SetMapVisitTime(uint32_t map, uint32_t t);   // @0x1983d8
uint32_t GetMapVisitTime(uint32_t map);           // @0x1983a4
bool HasMapVisitTime(uint32_t map);               // @0x195798
bool HasMapStartTime(uint32_t map);               // @0x195814
uint32_t GetMapEndTime(uint32_t map);             // @0x198414
void SetMapEndTime(uint32_t map, uint32_t t);     // @0x198448
void ResetBossFightsOnMap(uint32_t map);          // @0x19ba8c
// GameState::mBossFights / mBossFightsTimers (key: boss spawn id | map << 16): the enemies (entity id,
// HP; the boss first) of a boss fight under way and the time it was started.
std::vector<std::pair<uint32_t, uint32_t>>& BossFight(int key);   // (std::map::operator[])
bool HasBossFight(int key);
void EraseBossFight(int key);
uint32_t GetBossFightTimer(int key);              // 0 when there is none
bool HasBossFightTimer(int key);
void SetBossFightTimer(int key, uint32_t time);   // (insert: an existing one is kept, FUN_00118418)
void EraseBossFightTimer(int key);
uint32_t GetBossFightStartTime(uint32_t map, uint32_t spawn);   // @0x19160c
int GetMapCompletionCount(uint32_t map);          // @0x199590 how often a mission was finished
void AddMapCompletion(uint32_t map);              // @0x1995c4
uint32_t GetMonsterKilledCount(uint32_t id);      // @0x197a6c
void MonsterKilled(uint32_t id);                  // @0x197b24
bool GetCampaignCompletionStatus(uint32_t campaign);   // @0x197e54
void SetCampaignCompletionStatus(uint32_t campaign);   // @0x197e90
// @0x191ba0: the chapter number of the last completed chapter task (in id order; tasks 0x4e7..0x696
// count 13 further), 0 without one.
int GetLatestChapterCompleted();
uint32_t GetMapVersion(uint32_t map);             // @0x198088 (0 for a map never loaded)
void SetMapVersion(uint32_t map, uint32_t v);     // @0x198140
inline uint32_t GetGameMapVersion() { return 0xc; }   // @0x1909fc
// The game speed factor (0x60efc0, 1 at start): Set @0x191138 ignores values above 1, Get @0x191160,
// Reset @0x191174 puts back 1.
void SetTimeModifier(float f);
float GetTimeModifier();
void ResetTimeModifier();
bool IsTameTutorial();                         // @0x190bb4
bool IsFirstVirtualTutorial();                 // @0x190b44 second-tutorial steps 0x200..0x205
// @0x190b8c a PvP tutorial fight is running (the opening's ambush).
extern bool arenaBotActive;                    // 0x613520 the arenas' test bot (CombatTestBot)
bool IsPvPTutorial();                          // @0x190b8c (0x612d4c)
void SetPvPTutorial(bool on);                  // @0x190ba0 the ambush of the opening tutorial
void SetStartingArenaHP(int hp);               // @0x191504 (mStartingArenaHP)
int GetStartingArenaHP();
bool TaskCompleted(unsigned id);               // @0x1908ac (the 0x1000-entry lookup cache is not ported)
bool IsTaskStarted(unsigned id);               // @0x190988
uint32_t GetTaskBeginTime(unsigned id);        // @0x198624 (0 when not begun)
// @0x1904d8: the gold earned (0x6129bc); SetGoldEarned @0x1904bc has no caller in 5.11, so it stays 0.
inline int GetGoldEarned() { return 0; }
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
uint32_t GetFeaturedTaskClickTime(unsigned id);   // @0x198890 (0: never)
void SetFeaturedTaskClickTime(unsigned id, uint32_t t);   // @0x198f24
uint32_t GetEventStartTime(unsigned event);    // @0x198054 (0: never)
void SetEventStartTime(unsigned event, uint32_t t);   // @0x198018
uint32_t GetEventDifficulty(unsigned event);   // @0x197f74
uint32_t GetEventCompletionCount(unsigned event);   // @0x197f04 (std::map operator[])
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
unsigned GetSoldierReserveSlots();
// The tavern offers' state by TavernInfo id (chunk 0x4d): when the offer was last taken (its
// respawn timer starts there) and the HP the next warrior will have. 0 for an offer not seen yet.
uint32_t GetTavernWarriorTS(uint32_t id);             // @0x19adf8
void SetTavernWarriorTS(uint32_t id, uint32_t ts);    // @0x19add0
uint32_t GetTavernWarriorHP(uint32_t id);             // @0x19ae44
void SetTavernWarriorHP(uint32_t id, uint32_t hp);    // @0x19ae1c
void HireSoldier(Entity* e, bool spawn, int where);    // @0x19a15c where: 0 by the hero, 1 tavern, 2 barracks
void AddSoldiersToSquad(BaseSquad* squad, Entity* e);   // @0x191f8c spawned by the hero, then in the squad
void HireSoldier(int id);                              // @0x19a5d8        // @0x191d94 SoldierPool::GetTotalSlotsFree
void AddOfflineBuilding(const OfflineBuilding& b);   // @0x194928
void ClearOfflineBuildings();                  // @0x19bda8
OfflineBuilding* GetOfflineBuilding(unsigned i);       // @0x190f5c (unchecked)
unsigned GetOfflineBuildingCount();            // @0x1959e4
OfflineBuilding* GetOfflineBuildingWithID(uint32_t id);   // @0x194fec the first of that kind
inline void UpdateOfflineBuildings(float) {}   // @0x191320 (empty in 5.11)
// The goblins away on a decoration job (class 0x10, Entity::workType set), with the seconds left.
struct OfflineGoblin {          // 8 bytes (vector 0x612d9c)
    Entity* entity = nullptr;
    uint32_t timeLeft = 0;
};
OfflineGoblin* GetOfflineGoblin(unsigned i);   // @0x191324 (unchecked)
unsigned GetOfflineGoblinCount();              // @0x1952f8
void RemoveOfflineGoblin(Entity* e);           // @0x194bd4
void UpdateOfflineGoblins(float dt);           // @0x194c78 their times left, from Entity::SetEndWorkTS's
void ClearOfflineGoblins();                    // @0x19bd88
void RefreshOfflineGoblins();                  // @0x19c874 the working goblins not listed yet join
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
