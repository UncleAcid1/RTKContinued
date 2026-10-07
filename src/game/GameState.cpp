#include "game/GameState.h"

#include <cmath>
#include <cstdio>
#include <strings.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/SystemFuncs.h"
#include "engine/Timer.h"
#include "game/AIState.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/EntityManager.h"
#include "game/Combat.h"
#include "game/Items.h"
#include "game/SoldierSlots.h"
#include "game/Squad.h"
#include "game/SaveManager.h"
#include "game/Setting.h"
#include "game/StringTable.h"
#include "windows/Windows.h"

namespace GameState {

bool updated = true;             // 0x60efcc (a change to save)
int resourceAmountMax = 0;       // 0x6134a4 storage limit (CheckStorageFull)
int lastResourceAmountMax = 0;   // GameState::lastResourceAmountMax
int maxLevel = 0x1d;             // 0x60efc8
int totalGoldSpent = 0;          // 0x613418
int totalGoldEarned = 0;         // 0x61341c
int totalCrystalsSpent = 0;      // 0x613420
int totalXPEarned = 0;           // 0x613424
double totalTimeSpent = 0.0;     // 0x613410

int tutorial = 0;                // 0x6134ac
int secondTutorial = 0x81;       // 0x60efd0
int lastSentStep = 0;            // 0x6134b0
uint32_t playerSeed = 0;         // 0x613408
uint32_t mHPTS = 0;              // 0x613454
float mNextHP = 0.f;             // GameState::mNextHP seconds to the next regenerated HP
uint32_t latestUniqueID = 0;     // GameState::latestUniqueID (Building::SetUniqueID)

namespace {
// std::map<ResourceType,int> (tree header 0x612f78): every value v of type t is kept three times, as [t] = v,
// [t+0x2000] = v ^ [0xdd] and [t+0x4000] = v ^ [0xee], with the two keys stored in the same map.
std::map<int, uint32_t> g_res;
int g_resourceMismatches = 0;   // 0x612d60
uint32_t g_spent[11];           // 0x612970 per resource (ChangeResourceAmount)
uint32_t g_earned[11];          // 0x61299c
int g_crystalsEarned = 0;       // 0x6129c0 (AddCrystals)
int g_cityState = 0;        // 0/1: player's city (IsPlayerCity), 2+: visiting
int g_location = 0;
uint32_t g_mapId = 0;           // 0x6129cc
int g_pause = 0;                // 0x612d70
std::u32string g_castleName;    // 0x612f54
std::u32string g_playerName;    // 0x612d5c
bool g_male = true;             // 0x60efbc

using U32Map = std::map<uint32_t, uint32_t>;
std::vector<std::unique_ptr<PlayerItem>> g_items;   // 0x612f90 (records from the pool 0x612f68)
std::vector<std::unique_ptr<PlayerItem>> g_itemPool;   // 0x612f68 removed records (kept alive)
uint32_t g_itemUniqueId = 1;    // 0x60efc4 the highest item unique id
ItemBinding g_itemBindings[10];     // 0x612ad8
ItemBinding g_customBindings[32];   // 0x612b50 (customization)
bool g_beltItemMode = false;    // 0x612cd4
uint32_t g_belt[3];             // 0x612cd8 the belt's item ids
uint32_t g_beltActivation[3];   // 0x612ce4
uint32_t g_beltOld[3];          // 0x6133fc (chunk 0x1c, obsolete)
uint32_t g_beltSlotCount = 2;   // 0x60efb8
uint32_t g_beltSize = 0;        // 0x612cd0
std::vector<uint32_t> g_beltUniqueIds;   // 0x6133d8
std::set<uint32_t> g_completedTasks;     // 0x612cf4 TaskCompleted
U32Map g_subTasks;              // 0x612fe4 Set/GetSubTaskDone, AddSubTask
U32Map g_customSubtaskRequirement;   // 0x612ffc
std::set<uint32_t> g_startedTasks;       // 0x612d0c IsTaskStarted
std::map<int, std::vector<std::pair<uint32_t, uint32_t>>> g_bossFights;   // GameState::mBossFights
std::map<int, uint32_t> g_bossFightsTimers;   // GameState::mBossFightsTimers
U32Map g_mapCompletion;         // 0x613014 Get/AddMapCompletion
U32Map g_mapStartTime;          // 0x61302c
U32Map g_mapEndTime;            // 0x613044
U32Map g_mapVisitTime;          // 0x61305c
U32Map g_eventStartTime;        // 0x613074
U32Map g_eventEndTime;          // 0x61308c
U32Map g_eventDifficulty;       // 0x6130a4
U32Map g_eventCompletion;       // 0x6130bc
U32Map g_campaignCompletion;    // 0x6130d4
U32Map g_taskShowTime;          // 0x6130ec
U32Map g_taskBeginTime;         // 0x613104
U32Map g_taskEndTime;           // 0x61311c
U32Map g_mapCollectionInfo;     // 0x613134
U32Map g_professionPoints;      // 0x612d24
U32Map g_mapVersion;            // 0x61314c
std::map<uint32_t, std::pair<uint32_t, uint32_t>> g_warriorRespawn;   // 0x613164
U32Map g_itemBuff;              // 0x61317c Get/ActivateItemBuff
U32Map g_featuredTaskClickTime; // 0x613194
U32Map g_arenaStats[12];        // 0x6131ac, 0x6131c4, ... 0x6132b4
U32Map g_itemAcquireTime;       // 0x6132cc
U32Map g_itemRecentTime;        // 0x6132e4
U32Map g_soldierAcquireTime;    // 0x6132fc
U32Map g_soldierRecentTime;     // 0x613314
U32Map g_soldierAcquireCount;   // 0x61332c
U32Map g_chestItemReceived;     // 0x613344
U32Map g_chestBuildingReceived; // 0x61335c
U32Map g_chestEntityReceived;   // 0x613374
U32Map g_monsterKills;          // 0x612f9c
U32Map g_purchaseActive;        // 0x6133e4 (not cleared by Reset)
std::vector<OfflineBuilding> g_offlineBuildings;   // 0x612d64
uint32_t g_timeStart = 0;       // 0x612d98 GetGameStartTime
uint32_t g_tutorialType = 0;    // 0x6129d4
uint32_t g_dailyBonus[2];       // chunk 0x36
uint32_t g_dailyBonusCount = 0; // 0x612d84
uint32_t g_mainExpansionBackgroundLoad = 0;   // 0x612f44
bool g_socnetConnections[2];    // chunk 0x3c
// UNVERIFIED (offline port): the expansion pack versions the save keeps
// (FileManager::GetExpansionFileInfo(1 and 2) + 0x24 on the original; every pack ships with the
// port): kept as loaded.
uint32_t g_expansionVersions[2];
// The state of systems not ported yet, kept as the save holds it:
// UNVERIFIED (milestone 4): the presents container (PresentsContainer::Save/Load, chunk 8) is kept
// as loaded; a new game writes the empty container (no presents, 0x60f024 = 2, nothing opened).
std::vector<uint8_t> g_presents;
// Online state (Neighbours, PvP, PlayerProfileManager), kept as loaded.
char32_t g_neighbourSaveName[0x80];   // Neighbours::GetSaveName
uint32_t g_questFriendCount = 0;      // Neighbours::Get/SetQuestFriendCount
uint32_t g_pvpVictories = 0, g_pvpDefeats = 0;   // PvP::Get/SetVictoryCount, DefeatCount
uint32_t g_pvpTutorialLastRefresh = 0;   // PvP::Get/SetTutorialLastRefresh
bool g_pvpTutorialCompleted = false;     // PvP::IsCompletedPvPTutorial
struct PvPTutorialPlayer { uint32_t a; uint8_t b, c; };
std::vector<PvPTutorialPlayer> g_pvpTutorialPlayers;   // PvP::Get/SetTutorialPlayerStats
uint32_t g_playerType = 0;            // PlayerProfileManager::Get/SetPlayerType
std::string g_sid, g_guid;            // 0x612f28 / 0x612f2c (null until loaded: getSID / getGUID)
bool g_hasSid = false, g_hasGuid = false;
}

void Reset() {
    g_monsterKills.clear();
    g_res[0xdd] = 0x84358e6d;
    g_res[0xee] = 0x14a46840;
    SetResourceAmountValidated(kLevel, 1);
    g_resourceMismatches = 0;
    resourceAmountMax = 0;
    for (int i = 0; i < 0xb; ++i) {
        SetResourceAmountValidated(i, 0);
        g_spent[i] = 0;
        g_earned[i] = 0;
    }
    g_bossFights.clear();
    g_bossFightsTimers.clear();   // (and mArenaFights, not ported)
    SetResourceAmountValidated(kExpAfter, 1);
    updated = true;
    g_items.clear();
    g_completedTasks.clear();
    g_subTasks.clear();
    g_customSubtaskRequirement.clear();
    g_startedTasks.clear();
    for (U32Map* m : {&g_mapCompletion, &g_mapStartTime, &g_mapEndTime, &g_mapVisitTime, &g_eventStartTime,
                      &g_eventEndTime, &g_eventDifficulty, &g_eventCompletion, &g_campaignCompletion, &g_taskShowTime,
                      &g_taskBeginTime, &g_taskEndTime, &g_mapCollectionInfo, &g_professionPoints, &g_mapVersion})
        m->clear();
    g_warriorRespawn.clear();
    g_itemBuff.clear();
    g_featuredTaskClickTime.clear();
    for (U32Map& m : g_arenaStats) m.clear();
    uint32_t now = Timer::GetGlobalTime();
    g_timeStart = now;
    for (U32Map* m : {&g_itemAcquireTime, &g_itemRecentTime, &g_soldierAcquireTime, &g_soldierRecentTime,
                      &g_soldierAcquireCount, &g_chestItemReceived, &g_chestBuildingReceived, &g_chestEntityReceived})
        m->clear();
    g_beltItemMode = false;
    g_beltUniqueIds.clear();
    g_beltSlotCount = 2;
    for (ItemBinding& b : g_itemBindings) b = ItemBinding();
    for (ItemBinding& b : g_customBindings) b = ItemBinding();
    for (int i = 0; i < 3; ++i) g_belt[i] = g_beltActivation[i] = g_beltOld[i] = 0;
    // UNVERIFIED (milestone 4): Tasks::Reset.
    g_offlineBuildings.clear();
    tutorial = 0;
    secondTutorial = 0x81;
    SetPlayerGender(true);
    SetPlayerName(U"Lancelot");
    SetCastleName(U"");
    SoldierPool::SetFreeSlotsCount((unsigned)Setting("reserve_slot_start").GetInt());
    SoldierPool::SetMaxSlotsCount((unsigned)Setting("reserve_slot_max").GetInt());
    // UNVERIFIED (milestone 4): mVirtualHealth and the other combat and event fields Reset clears.
    g_dailyBonusCount = 0;
    g_tutorialType = 0;
    g_presents.clear();
    g_pause = 0;
}

// A tampered copy is outvoted by the other two; with no two agreeing, 0 (1 for the level).
uint32_t GetResourceAmount(int type) {
    uint32_t v = g_res[type];
    uint32_t a = g_res[0xdd] ^ g_res[type + 0x2000];
    uint32_t b = g_res[0xee] ^ g_res[type + 0x4000];
    if (v == a && v == b) return v;
    ++g_resourceMismatches;
    if (a == b) return g_res[type] = a;
    if (v == b) g_res[type + 0x2000] = v ^ g_res[0xdd];
    else if (v == a) g_res[type + 0x4000] = v ^ g_res[0xee];
    else v = type == kLevel ? 1 : 0;
    return v;
}

void SetResourceAmountValidated(int type, uint32_t v) {
    g_res[type] = v;
    g_res[type + 0x2000] = g_res[0xdd] ^ v;
    g_res[type + 0x4000] = g_res[0xee] ^ v;
}
uint32_t GetResourceAmountValidated(int type) { return GetResourceAmount(type); }
int GetLevel() { return (int)GetResourceAmount(kLevel); }

bool CheckStorageFull(int type) { return resourceAmountMax <= (int)GetResourceAmount(type); }

// PORT: Expansions::AreEnabled is false (every pack ships with the port), so the original's cap on
// XP below level 4 until the mandatory packs are installed never applies. combatTestEnabled
// (0x6124a6, a debug switch that zeroes every change) is off.
void ChangeResourceAmount(int type, int amount) {
    updated = true;
    if (type == kCrystal && amount > 0) {
        std::fprintf(stderr, "ERROR: GameState::ChangeResourceAmount() Cannot add crystals using this function\n");
        return;
    }
    SetResourceAmountValidated(type, GetResourceAmount(type) + amount);
    if (amount < 1) {
        g_spent[type] -= amount;
        if (type == kGold) { totalGoldSpent -= amount; return; }
        if (type == kCrystal) { totalCrystalsSpent -= amount; return; }
    } else {
        g_earned[type] += amount;
        if (type == kGold) { totalGoldEarned += amount; return; }
    }
    if (type == kExpAfter) {
        totalXPEarned += amount;
        int before = (int)GetResourceAmount(kLevel);
        // (Past the last level_exp_N the table holds 0 and the original reads on; unreachable in play.)
        while ((int)GetResourceAmount(kExpAfter) >= (int)GetLevelXP(GetResourceAmount(kLevel)))
            SetResourceAmountValidated(kLevel, GetResourceAmount(kLevel) + 1);
        if ((int)GetResourceAmount(kLevel) > maxLevel) SetResourceAmountValidated(kLevel, maxLevel);
        if (before != (int)GetResourceAmount(kLevel)) {
            // UNVERIFIED (milestone 4): Tasks::CompleteSubtask(7, 0, 1).
            LevelUpWindow::Show();
        }
        return;
    }
    if (type > kOil) return;
    if (GetCurrentMapID() == 0) {
        // UNVERIFIED (milestone 3c): Map::UpdateStorages().
    }
}

void AddCrystals(int amount) {
    if (amount < 0)
        std::fprintf(stderr, "ERROR: GameState::AddCrystals(%d) Function was called with negative number as argument\n", amount);
    updated = true;
    SetResourceAmountValidated(kCrystal, GetResourceAmount(kCrystal) + amount);
    if (amount > 0) g_crystalsEarned += amount;
    else g_spent[kCrystal] -= amount;
}

const char* GetResourceName(int type) {
    static const char* names[] = {"LUMBER", "ROCKS", "FOOD", "PLANKS", "STONES", "MEAT", "SAUSAGE",
                                  "OIL", "GOLD", "CRYSTAL", "EXP_AFTER"};
    return type >= 0 && type < 11 ? names[type] : "";
}

int ExternalResourceTypeToInternal(unsigned type) {   // table 0x580a4c (gold and crystal first)
    static const int table[] = {kGold, kCrystal, kLumber, kRocks, kFood, kPlanks, kStones, kMeat, kSausage, kOil, kExpAfter};
    return table[type];   // (no range check on the original either)
}

int StringToResourceType(const char* name) {
    static const struct { const char* name; int type; } table[] = {
        {"LUMBER", kLumber}, {"ROCKS", kRocks}, {"FOOD", kFood}, {"PLANKS", kPlanks}, {"STONES", kStones},
        {"MEAT", kMeat}, {"SAUSAGE", kSausage}, {"SAUSAGES", kSausage}, {"OIL", kOil}, {"GOLD", kGold},
        {"CRYSTAL", kCrystal}, {"XP", kExpAfter}, {"EXP", kExpAfter}};
    for (const auto& e : table)
        if (strcasecmp(name, e.name) == 0) return e.type;
    std::fprintf(stderr, "ERROR: GameState::StringToResourceType() Unknown resource type %s", name);
    return kGold;
}

bool IsPlayerCity() { return (unsigned)g_cityState <= 1; }
int GetCurrentLocation() { return g_location; }
void SetCurrentLocation(int location) { g_location = location; }
uint32_t GetCurrentMapID() { return g_mapId; }
void SetCurrentMapID(uint32_t id) {
    g_location = id != 0 ? 2 : 0;   // SetCurrentLocation: campaign maps / city
    g_mapId = id;
}
bool IsTutorial() { return tutorial < 0x18; }
// @0x1927bc: the city's own people (entity class 0, on the city map, not NPCs).
int GetPlayerWorkersCount() {
    int n = 0;
    for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i)
        if (e->data->clas == 0 && e->GetCurrentMap() == 0 && !e->IsNPC()) ++n;
    return n;
}

// @0x19a7a8: Setting "max_population_per_level" at the player's level (the last entry beyond).
int GetMaxWorkerCount() {
    unsigned i = (unsigned)(GetLevel() - 1);
    Setting s("max_population_per_level");
    if (s.GetChildrenCount() <= i) i = s.GetChildrenCount() - 1;
    return s.GetChild(i).GetInt();
}
bool IsMalePlayer() { return g_male; }
void SetPlayerGender(bool male) { g_male = male; }
const char32_t* GetPlayerName() { return g_playerName.c_str(); }
void SetPlayerName(const char32_t* name) { g_playerName = name; }

const char32_t* GetCastleName() {
    if (!g_castleName.empty()) return g_castleName.c_str();
    return StringTable::GetString("world_node_player_no_name");
}
void SetCastleName(const char32_t* name) { g_castleName = name; }
int TutorialStep() { return tutorial; }
int SecondTutorialStep() { return secondTutorial; }

bool IsCityTutorial() {
    if (TutorialStep() <= 0x67) return true;
    unsigned second = (unsigned)SecondTutorialStep();
    if (second - 0x8au <= 0x75u) return true;
    return second - 0x82u < 7u;
}

bool IsTameTutorial() {
    if (tutorial == 0x17) return true;
    if (tutorial == 0x80 && g_mapId == 0xd) return !TaskCompleted(0x771);
    return false;
}
bool IsPvPTutorial() { return false; }
bool IsFirstVirtualTutorial() { return (unsigned)(secondTutorial - 0x200) < 6; }

bool TaskCompleted(unsigned id) { return g_completedTasks.count(id) != 0; }
bool IsTaskStarted(unsigned id) { return g_startedTasks.count(id) != 0; }
uint32_t GetTaskBeginTime(unsigned id) {
    auto it = g_taskBeginTime.find(id);
    return it == g_taskBeginTime.end() ? 0 : it->second;
}
uint32_t GetGameStartTime() { return g_timeStart; }

namespace {
std::vector<std::unique_ptr<Order>> g_orders;   // 0x612d50 (pool-allocated on the original)
std::vector<std::unique_ptr<Order>> g_orderPool; // freed orders (the pool's free list)
}

void PlaceOrder(Map::Building* from, Map::Building* to, int amount, int type, int kind) {
    auto o = std::make_unique<Order>();
    o->amount = amount;
    o->from = from;
    o->to = to;
    o->type = type;
    o->goblin = nullptr;
    o->taken = false;
    o->kind = kind;
    g_orders.push_back(std::move(o));
}

Order* GetTopOrder() {
    if (g_orders.empty()) return nullptr;
    int amounts[11];
    for (int t = 0; t < 11; ++t) amounts[t] = (int)GetResourceAmount(t);
    for (size_t i = g_orders.size(); i-- > 0;) {
        Order* o = g_orders[i].get();
        if (o->taken) continue;
        if (o->kind == 0 && resourceAmountMax <= amounts[o->type]) continue;
        o->taken = true;
        return o;
    }
    return nullptr;
}

int GetOrderCount(int type) {
    int n = 0;
    for (auto& o : g_orders)
        if (o->type == type && o->goblin) ++n;
    return n;
}

void RemoveAllOrders(Map::Building* from, int n) {
    for (size_t i = 0; i < g_orders.size();) {
        if (n < 0) return;
        Order* o = g_orders[i].get();
        if (o->from != from) { ++i; continue; }
        if (o->goblin && o->goblin->GetAI()) o->goblin->GetAI()->CancelWork();
        g_orderPool.push_back(std::move(g_orders[i]));
        g_orders.erase(g_orders.begin() + i);
        --n;
    }
}

void RemoveAllTargetOrders(Map::Building* to, int n) {
    for (size_t i = 0; i < g_orders.size();) {
        if (n < 0) return;
        if (g_orders[i]->to != to) { ++i; continue; }
        g_orderPool.push_back(std::move(g_orders[i]));
        g_orders.erase(g_orders.begin() + (long)i);
        --n;
    }
}

void RemoveOrderOfWorker(Entity* goblin) {
    for (size_t i = 0; i < g_orders.size(); ++i) {
        if (g_orders[i]->goblin != goblin) continue;
        g_orderPool.push_back(std::move(g_orders[i]));
        g_orders.erase(g_orders.begin() + (long)i);
        return;
    }
}

void CancelWork(int type) {
    for (size_t i = 0; i < g_orders.size(); ++i) {
        Order* o = g_orders[i].get();
        if (o->type == type && o->goblin) o->goblin->GetAI()->CancelWork();
    }
}

int GetTutorialType() { return (int)g_tutorialType; }

int GetGoblinCount() {
    int n = 0;
    for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i)
        if (e->GetEntityData()->clas == 0x10) ++n;
    return n;
}

int AdjustCrystalCost(int cost) { return (int)((float)cost * GetSetting("crystal_mult")); }

const char* GetResourceIconName(int type) {
    static const char* const kIcons[] = {
        "Icon_16_wood", "Icon_16_rock", "Icon_16_food", "Icon_16_plank", "Icon_16_cut_stone", "Icon_16_meat",
        "Icon_16_sauage", "Icon_16_oil", "Icon_16_gold", "Icon_16_crystal", "Icon_xp"};
    return kIcons[type];
}

const char* GetResourceWorkerIconName(int type) {
    static const char* const kIcons[] = {
        "Icon_profession_lumberjack", "Icon_profession_miner", "", "", "Icon_profession_stone_cutter", "", "", "", "",
        "", ""};
    return kIcons[type];
}

const char* GetFarmDropImageName(unsigned id) {
    switch (id) {
    case 10: return "images/Items/FarmDrops/res_crrot_drop";
    case 0xc: return "images/Items/FarmDrops/res_pumpkin_drop";
    case 0xd: return "images/Items/FarmDrops/res_corn_drop";
    case 0xe: return "images/Items/FarmDrops/res_cabbage_drop";
    case 0x1e: return "images/Items/FarmDrops/res_canola_drop";
    case 0x1f: return "images/Items/FarmDrops/res_flax_drop";
    case 0x20: return "images/Items/FarmDrops/res_sunflower_drop";
    case 0x21: return "images/Items/FarmDrops/res_sesame_drop";
    case 0x22: return "images/Items/FarmDrops/res_peanut_drop";
    case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: return "images/Items/quest_items/icon_quest_meat";
    default: return "images/Items/FarmDrops/res_wheat_drop";
    }
}

const char* GetResourceMapIconName(int type) {
    static const char* const kIcons[] = {
        "images/Resources/lumber/stage_5", "images/Resources/rocks/stage_5", "Icon_60_food",
        "images/Resources/planks/stage_5", "images/Resources/stones/stage_5", "images/Resources/meat/stage_5",
        "images/Resources/sausage/stage_5", "images/Resources/oil/stage_5", "Icon_bugs", "Icon_crystal_60",
        "Icon_xp"};
    return kIcons[type];
}

const char32_t* GetResourceGameName(int type) {
    static const char* const kNames[] = {"RES_LUMBER", "RES_ROCKS", "REC_FOOD", "RES_PLANKS", "RES_STONES",
        "RES_MEAT", "RES_SAUSAGE", "RES_OIL", "RES_GOLD", "RES_CRYSTAL", "EXP_AFTER"};
    return StringTable::GetString(kNames[type]);
}

void RaiseGamePauseState() { ++g_pause; }
void DropGamePauseState() {
    if (--g_pause < 0) g_pause = 0;
}
bool IsPaused() { return g_pause > 0; }

// ----------------------------------------------------------------------------------------- items
uint32_t GetBeltSlotCount() { return g_beltSlotCount; }
void SetBeltSlotCount(uint32_t n) { g_beltSlotCount = n > 3 ? 3 : n; }
void SetBeltSize(uint32_t n) { g_beltSize = n; }
uint32_t GetBeltSize() { return g_beltSize; }
void SetBeltItemMode(bool on) { g_beltItemMode = on; }
bool GetBeltItemMode() { return g_beltItemMode; }

void BindBeltItemTo(unsigned slot, uint32_t id) {
    g_beltActivation[slot] = 0;
    g_belt[slot] = id;
}

void SetBeltItemActivationTimeAt(unsigned slot, uint32_t t) { g_beltActivation[slot] = t; }
uint32_t GetBeltItemActivationTimeAt(unsigned slot) { return g_beltActivation[slot]; }
void ClearBeltItems() { g_beltUniqueIds.clear(); }
void AddBeltItem(uint32_t uniqueId) { g_beltUniqueIds.push_back(uniqueId); }

PlayerItem* GetItemAt(unsigned binding) { return g_itemBindings[binding].item; }
PlayerItem* GetCustomizationAt(unsigned binding) { return g_customBindings[binding].item; }

void BindItemTo(unsigned binding, PlayerItem* item) {
    g_itemBindings[binding].item = item;
    g_itemBindings[binding].uniqueId = item ? item->uniqueId : 0;
}

void BindCustomizationTo(unsigned binding, PlayerItem* item) {
    g_customBindings[binding].item = item;
    g_customBindings[binding].uniqueId = item ? item->uniqueId : 0;
}

bool IsItemBinded(const PlayerItem* item) {
    if (!item || item->info->slot == 0) return false;
    for (const ItemBinding& b : g_itemBindings)
        if (b.uniqueId == item->uniqueId) return true;
    return false;
}

// UNVERIFIED (milestone 4c): Tasks::CompleteSubtask(0x20, id, 1) (the "get item" quest steps).
// UNVERIFIED (milestone 5): an item of a set counts the set's parts (Sets::GetSetForItem,
// FindInventorySetState, Tasks::CompleteSubtask(0x28, ...)). Not ported (online): the OG / OG2
// sharing requests (a complete set, a new gem or orb, new equipment unless noShare).
PlayerItem* AddItem(uint32_t id, int count, bool flag14, bool timesOnly, bool noShare) {
    if (count < 1) {
        std::fprintf(stderr, "ERROR: GameState::AddItem() Item %d count is %d\n", id, count);
        return nullptr;
    }
    Items::ItemInfo* info = Items::GetItemInfo(id);
    if (!info) return nullptr;
    PlayerItem* last = nullptr;
    if (!timesOnly) {
        for (int i = 0; i < count; ++i) {
            auto it = std::make_unique<PlayerItem>();
            it->info = info;
            it->id = id;
            it->uniqueId = ++g_itemUniqueId;
            it->f0c = it->f10 = info->durability;
            it->f14 = flag14;
            last = it.get();
            g_items.push_back(std::move(it));
        }
    }
    if (!g_itemAcquireTime.count(id)) g_itemAcquireTime[id] = (uint32_t)Timer::GetGlobalTime();
    g_itemRecentTime[id] = (uint32_t)Timer::GetGlobalTime();
    return last;
}

void RemoveUniqueItem(uint32_t uniqueId) {
    for (size_t i = 0; i < g_items.size(); ++i) {
        if (g_items[i]->uniqueId != uniqueId) continue;
        g_itemPool.push_back(std::move(g_items[i]));
        g_items[i] = std::move(g_items.back());
        g_items.pop_back();
        return;
    }
    std::fprintf(stderr, "ERROR: GameState::RemoveUniqueItem() Could not remove an item with unique ID = %d\n", uniqueId);
}

uint32_t GetItemBuffRemainingTime(uint32_t id) {
    uint32_t end = g_itemBuff[id];
    uint32_t now = (uint32_t)Timer::GetGlobalTime();
    return end == 0 || end < now ? 0 : end - now;
}

void ActivateItemBuff(uint32_t id, uint32_t seconds) { g_itemBuff[id] = (uint32_t)Timer::GetGlobalTime() + seconds; }

uint32_t GetItemRecentTime(uint32_t id) {
    auto it = g_itemRecentTime.find(id);
    return it == g_itemRecentTime.end() ? 0 : it->second;
}

uint32_t GetItemAcquirementTime(uint32_t id) {
    auto it = g_itemAcquireTime.find(id);
    return it == g_itemAcquireTime.end() ? 0 : it->second;
}

// The player's HP from the time since mHPTS: one HP per GetHPRegeneration seconds (mHPTS starts
// a full bar back); mNextHP is the wait for the next one.
void SetupPlayerRegenerationState() {
    Entity* player = EntityManager::GetPlayer();
    if (!player) return;
    if (mHPTS == 0) mHPTS = (uint32_t)(Timer::GetGlobalTime() - player->GetHpMax() * player->GetHPRegeneration());
    int elapsed = Timer::GetGlobalTime() - (int)mHPTS;
    int regen = player->GetHPRegeneration();
    int hp = (int)std::floor((float)elapsed / (float)regen);
    mNextHP = (float)(regen - elapsed % regen);
    if (player->GetHpMax() < hp) {
        hp = player->GetHpMax();
        mNextHP = 0.f;
    }
    if (hp < player->GetHP()) {
        std::fprintf(stderr, "ERROR: GameState::SetupPlayerRegenerationState() After regeneration, player HP has decreased from %d to %d\n",
                     player->GetHP(), hp);
        hp = player->GetHP();
    }
    player->SetHP(hp);
    if (player->GetHP() != player->GetHpMax()) return;
    if (IsTaskStarted(0x2f9)) {
        // UNVERIFIED (milestone 4c): Tasks::CompleteSubtask(0x27, 1, 1).
    }
}

void UpdatePlayerRegenerationState() {
    Entity* player = EntityManager::GetPlayer();
    if (!player) return;
    mHPTS = (uint32_t)(Timer::GetGlobalTime() - player->GetHP() * player->GetHPRegeneration());
    if (mNextHP == 0.f) mNextHP = (float)player->GetHPRegeneration();
}
uint32_t GetBeltItemAt(unsigned slot) { return g_belt[slot]; }

// With the belt item mode on, an item on the belt counts (and is found) only among the belt's own
// copies (unique ids 0x6133d8).
int GetItemAmount(uint32_t id, bool belt) {
    bool onBelt = g_belt[0] == id || g_belt[1] == id || g_belt[2] == id;
    int n = 0;
    for (auto& it : g_items) {
        if (it->id != id) continue;
        if (!g_beltItemMode || !belt || !onBelt) { ++n; continue; }
        for (uint32_t u : g_beltUniqueIds)
            if (it->uniqueId == u) { ++n; break; }
    }
    return n;
}

// Each removal moves the last item into the freed slot.
void RemoveItem(uint32_t id, int count) {
    if (count < 1) {
        std::fprintf(stderr, "ERROR: GameState::RemoveItem() Item %d count is %d\n", id, count);
        return;
    }
    for (size_t i = 0; i < g_items.size() && count != 0;) {
        if (g_items[i]->id != id) {
            ++i;
            continue;
        }
        g_itemPool.push_back(std::move(g_items[i]));
        g_items[i] = std::move(g_items.back());
        g_items.pop_back();
        --count;
    }
    if (count != 0)
        std::fprintf(stderr, "ERROR: GameState::RemoveItem() Could not remove %d items with ID = %d - player has no items with this ID\n",
                     count, id);
}

PlayerItem* GetItemByUniqueID(uint32_t uniqueId) {
    for (auto& it : g_items)
        if (it->uniqueId == uniqueId) return it.get();
    return nullptr;
}

PlayerItem* GetFirstItemByID(uint32_t id, bool belt) {
    for (auto& it : g_items) {
        if (it->id != id) continue;
        if (!g_beltItemMode || !belt) return it.get();
        if (id != g_belt[0] && id != g_belt[1] && id != g_belt[2]) return it.get();
        for (uint32_t u : g_beltUniqueIds)
            if (u == it->uniqueId) return it.get();
    }
    return nullptr;
}

PlayerItem* EnumItems(unsigned i) { return i < g_items.size() ? g_items[i].get() : nullptr; }

uint32_t ExternalItemBindingToInternal(uint32_t slot) {
    static const uint32_t kInternal[10] = {0, 5, 1, 6, 2, 7, 3, 8, 4, 9};
    return kInternal[slot - 1];
}

uint32_t InternalItemBindingToExternal(uint32_t slot) {
    static const uint32_t kExternal[10] = {1, 3, 5, 7, 9, 2, 4, 6, 8, 10};
    return kExternal[slot];
}

void AddOfflineBuilding(const OfflineBuilding& b) { g_offlineBuildings.push_back(b); }
void ClearOfflineBuildings() { g_offlineBuildings.clear(); }

// ---------------------------------------------------------------------------------- saving
namespace {
using SaveManager::BeginChunk;
using SaveManager::EndChunk;
using SaveManager::SaveChar;
using SaveManager::SaveShort;
using SaveManager::SaveUnsigned;

template <class M>
void SaveMap(const M& m) {
    SaveUnsigned((uint32_t)m.size());
    for (auto& kv : m) {
        SaveUnsigned((uint32_t)kv.first);
        SaveUnsigned(kv.second);
    }
}

void LoadMap(SaveManager::Reader& r, U32Map& m) {
    uint32_t n = r.u32();
    for (uint32_t i = 0; i < n; ++i) {
        uint32_t k = r.u32();
        m[k] = r.u32();
    }
}

void SaveString(const std::u32string& s) {   // a length and one u32 per character
    SaveUnsigned((uint32_t)s.size());
    for (char32_t c : s) SaveUnsigned((uint32_t)c);
}

std::u32string LoadString(SaveManager::Reader& r) {
    uint32_t n = r.u32();
    std::u32string s;
    for (uint32_t i = 0; i < n; ++i) s += (char32_t)r.u32();
    return s;
}

// The 32-byte key the SID and GUID are stored with: k[0] = 0x3d, k[i + 1] = k[i] * 0x10d >> 7.
void SidKey(uint8_t k[32]) {
    k[0] = 0x3d;
    for (int i = 0; i < 31; ++i) k[i + 1] = (uint8_t)((int)(k[i] * 0x10d) >> 7);
}

std::unique_ptr<SaveManager::SaveBlock> g_entityDataCopy;   // GameState::entityDataCopy
}  // namespace

void Save(uint32_t version) {
    BeginChunk(SaveManager::kStateBase);
    SaveUnsigned(version);
    SaveUnsigned(playerSeed);
    SaveUnsigned(GetResourceAmountValidated(kLevel));
    SaveChar(0xb);
    for (int i = 0; i < 0xb; ++i) {
        SaveUnsigned(GetResourceAmountValidated(i));
        SaveUnsigned(g_earned[i]);
        SaveUnsigned(g_spent[i]);
    }
    SaveUnsigned((uint32_t)g_completedTasks.size());
    for (uint32_t t : g_completedTasks) SaveUnsigned(t);
    SaveMap(g_subTasks);
    SaveUnsigned(0);
    EndChunk();
    BeginChunk(0xf);
    SaveUnsigned(Timer::GetTimeAdvance());
    EndChunk();
    BeginChunk(0x10);
    SaveUnsigned(g_male);
    SaveString(g_playerName);
    SaveString(g_castleName);
    EndChunk();
    BeginChunk(0x3a);
    SaveUnsigned((uint32_t)g_items.size());
    for (auto& it : g_items) {
        SaveUnsigned(it->id);
        SaveUnsigned(it->uniqueId);
        SaveUnsigned(it->f0c);
        SaveUnsigned(it->f10);
        SaveUnsigned(it->f14);
    }
    SaveUnsigned(10);
    for (const ItemBinding& b : g_itemBindings) SaveUnsigned(b.uniqueId);
    EndChunk();
    BeginChunk(0x12);
    SaveUnsigned(0);
    SaveUnsigned(0);
    SaveUnsigned(0);
    EndChunk();
    BeginChunk(0x15);
    SaveUnsigned(3);
    for (uint32_t v : g_belt) SaveUnsigned(v);
    EndChunk();
    BeginChunk(0x16);
    SaveUnsigned(g_mapId);
    EndChunk();
    BeginChunk(0x18);
    SaveUnsigned((uint32_t)g_startedTasks.size());
    for (uint32_t t : g_startedTasks) SaveUnsigned(t);
    EndChunk();
    BeginChunk(0x19);
    SaveUnsigned((uint32_t)g_bossFights.size());
    for (auto& kv : g_bossFights) {
        SaveUnsigned((uint32_t)kv.first);
        SaveUnsigned((uint32_t)kv.second.size());
        for (auto& pr : kv.second) {
            SaveUnsigned(pr.first);
            SaveUnsigned(pr.second);
        }
    }
    EndChunk();
    BeginChunk(0x1a);
    SaveMap(g_mapCompletion);
    EndChunk();
    BeginChunk(0x1b);
    SaveUnsigned(3);
    for (uint32_t v : g_beltActivation) SaveUnsigned(v);
    EndChunk();
    BeginChunk(0x1c);
    SaveUnsigned(3);
    for (uint32_t v : g_beltOld) SaveUnsigned(v);
    EndChunk();
    BeginChunk(0x1d);
    SaveMap(g_mapStartTime);
    EndChunk();
    BeginChunk(0x27);
    SaveMap(g_mapEndTime);
    EndChunk();
    BeginChunk(0x1e);
    SaveUnsigned((uint32_t)tutorial);
    EndChunk();
    BeginChunk(0x24);
    SaveUnsigned((uint32_t)secondTutorial);
    EndChunk();
    BeginChunk(0x1f);
    SaveUnsigned(mHPTS);
    EndChunk();
    BeginChunk(0x20);
    SaveUnsigned((uint32_t)g_offlineBuildings.size());
    for (const OfflineBuilding& b : g_offlineBuildings) {
        SaveUnsigned(b.id);
        SaveUnsigned(b.level);
        SaveUnsigned(b.contract);
        SaveUnsigned(b.flags);
        SaveUnsigned(b.stateTime);
        SaveChar(b.f14);
        SaveChar(b.f15);
        SaveUnsigned(b.x);
        SaveUnsigned(b.y);
    }
    EndChunk();
    BeginChunk(0x21);
    SaveMap(g_taskShowTime);
    EndChunk();
    BeginChunk(0x22);
    SaveMap(g_mapCollectionInfo);
    EndChunk();
    BeginChunk(0x29);
    SaveUnsigned(SoldierPool::GetTotalSlotsFree());
    EndChunk();
    BeginChunk(0x2b);
    SaveUnsigned(g_timeStart);
    EndChunk();
    BeginChunk(0x2c);
    SaveMap(g_itemAcquireTime);
    EndChunk();
    BeginChunk(0x2e);
    SaveMap(g_itemRecentTime);
    EndChunk();
    // PresentsContainer::Save
    BeginChunk(8);
    if (g_presents.empty()) {
        SaveUnsigned(0);
        SaveUnsigned(0);
        SaveUnsigned(2);
        SaveUnsigned(0);
    } else {
        for (uint8_t c : g_presents) SaveChar(c);
    }
    EndChunk();
    BeginChunk(0x34);
    SaveMap(g_bossFightsTimers);
    EndChunk();
    BeginChunk(0x35);
    SaveMap(g_taskBeginTime);
    EndChunk();
    BeginChunk(0x36);
    SaveUnsigned(g_dailyBonus[0]);
    SaveUnsigned(g_dailyBonus[1]);
    EndChunk();
    BeginChunk(0x37);
    SaveShort(0xff);
    SaveShort(0xff);
    EndChunk();
    BeginChunk(0x38);
    SaveMap(g_campaignCompletion);
    EndChunk();
    BeginChunk(0x39);
    SaveMap(g_taskEndTime);
    EndChunk();
    BeginChunk(0x3b);
    SaveUnsigned(1);
    EndChunk();
    BeginChunk(0x3c);
    SaveUnsigned(g_socnetConnections[0]);
    SaveUnsigned(g_socnetConnections[1]);
    EndChunk();
    BeginChunk(0x3d);
    SaveUnsigned(g_tutorialType);
    EndChunk();
    BeginChunk(0x3e);
    SaveUnsigned(g_expansionVersions[0]);
    SaveUnsigned(g_expansionVersions[1]);
    EndChunk();
    BeginChunk(0x3f);
    SaveUnsigned(g_mainExpansionBackgroundLoad);
    EndChunk();
    BeginChunk(0x40);
    SaveUnsigned(g_dailyBonusCount);
    EndChunk();
    BeginChunk(0x41);
    SaveMap(g_professionPoints);
    EndChunk();
    BeginChunk(0x43);
    SaveUnsigned((uint32_t)g_resourceMismatches);
    EndChunk();
    BeginChunk(0x46);
    SaveMap(g_eventStartTime);
    EndChunk();
    BeginChunk(0x47);
    SaveMap(g_eventEndTime);
    EndChunk();
    BeginChunk(0x48);
    SaveMap(g_mapVersion);
    EndChunk();
    BeginChunk(0x49);
    SaveMap(g_eventDifficulty);
    EndChunk();
    BeginChunk(0x4a);
    SaveMap(g_eventCompletion);
    EndChunk();
    BeginChunk(0x4b);
    SaveMap(g_monsterKills);
    EndChunk();
    BeginChunk(0x4c);
    for (const U32Map& m : g_arenaStats) SaveMap(m);
    EndChunk();
    BeginChunk(0x4d);
    SaveUnsigned((uint32_t)g_warriorRespawn.size());
    for (auto& kv : g_warriorRespawn) {
        SaveUnsigned(kv.first);
        SaveUnsigned(kv.second.first);
        SaveUnsigned(kv.second.second);
    }
    EndChunk();
    BeginChunk(0x4e);
    int len = 0;
    while (len < 0x7f && g_neighbourSaveName[len]) ++len;
    SaveShort((int16_t)len);
    for (int i = 0; i < 0x7f; ++i) SaveShort((int16_t)((uint16_t)g_neighbourSaveName[i] ^ 0xaaaa));
    SaveUnsigned(g_questFriendCount);
    EndChunk();
    BeginChunk(0x4f);
    SaveUnsigned(GetResourceAmountValidated(kLevel));
    SaveUnsigned(10);
    for (const ItemBinding& b : g_itemBindings) {
        SaveUnsigned(b.uniqueId);
        if (b.uniqueId == 0) continue;
        // (the original reads the bound item without a null check)
        SaveUnsigned(b.item ? b.item->id : 0);
        SaveUnsigned(b.item ? b.item->f0c : 0);
        SaveUnsigned(b.item ? b.item->f10 : 0);
    }
    SaveUnsigned(GetBeltSlotCount());
    for (unsigned i = 0; i < GetBeltSlotCount(); ++i) {
        uint32_t id = GetBeltItemAt(i);
        SaveUnsigned(id);
        if (id) SaveUnsigned((uint32_t)GetItemAmount(id, true));
    }
    uint32_t soldiers = 0;
    for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i)
        if (e->GetEntityData()->clas == 10 || e->player) ++soldiers;
    SaveUnsigned(soldiers);
    for (unsigned i = 0; soldiers && EntityManager::EnumEntities(i); ++i) {
        Entity* e = EntityManager::EnumEntities(i);
        if (e->GetEntityData()->clas != 10 && !e->player) continue;
        SaveShort((int16_t)e->GetEntityData()->id);
        SaveShort((int16_t)e->GetHP());
        SaveChar(e->f3b);
        SaveShort((int16_t)e->GetAP());
        SaveShort((int16_t)e->GetHpMax());
    }
    SaveUnsigned(g_pvpVictories);
    SaveUnsigned(g_pvpDefeats);
    SaveUnsigned(0x20);
    for (const ItemBinding& b : g_customBindings) {
        PlayerItem* it = GetItemByUniqueID(b.uniqueId);
        SaveUnsigned(it ? it->id : 0);
    }
    EndChunk();
    BeginChunk(0x56);
    SaveUnsigned(g_pvpTutorialLastRefresh);
    SaveChar(g_pvpTutorialCompleted);
    SaveUnsigned((uint32_t)g_pvpTutorialPlayers.size());
    for (const PvPTutorialPlayer& pl : g_pvpTutorialPlayers) {
        SaveUnsigned(pl.a);
        SaveChar(pl.b);
        SaveChar(pl.c);
    }
    EndChunk();
    BeginChunk(0x68);
    SaveUnsigned(g_playerType);
    EndChunk();
    BeginChunk(0x51);
    SaveMap(g_customSubtaskRequirement);
    EndChunk();
    BeginChunk(0x52);
    SaveMap(g_soldierAcquireTime);
    SaveMap(g_soldierRecentTime);
    SaveMap(g_soldierAcquireCount);
    EndChunk();
    BeginChunk(0x54);
    SaveUnsigned(0x20);
    for (const ItemBinding& b : g_customBindings) {
        PlayerItem* it = GetItemByUniqueID(b.uniqueId);
        SaveUnsigned(it ? it->id : 0);
    }
    EndChunk();
    BeginChunk(0x55);
    SaveMap(g_mapVisitTime);
    EndChunk();
    BeginChunk(0x57);
    SaveMap(g_itemBuff);
    EndChunk();
    BeginChunk(0x58);
    SaveMap(g_chestItemReceived);
    SaveMap(g_chestBuildingReceived);
    SaveMap(g_chestEntityReceived);
    EndChunk();
    BeginChunk(0x59);
    SaveMap(g_featuredTaskClickTime);
    EndChunk();
    BeginChunk(0x60);
    SaveMap(g_purchaseActive);
    EndChunk();
    BeginChunk(0x66);
    uint8_t key[32];
    SidKey(key);
    std::string sid = g_hasSid ? g_sid : SystemFuncs::getSID();
    SaveUnsigned((uint32_t)sid.size());
    for (size_t i = 0; i < sid.size(); ++i) SaveChar(key[i & 0x1f] ^ (uint8_t)sid[i]);
    std::string guid = g_hasGuid ? g_guid : SystemFuncs::getGUID();
    SaveUnsigned((uint32_t)guid.size());
    for (size_t i = 0; i < guid.size(); ++i) SaveChar(key[i & 0x1f] ^ (uint8_t)guid[i]);
    EndChunk();
    SaveEntities();
    BeginChunk(SaveManager::kEnd);
    SaveUnsigned(0);
    EndChunk();
}

void Load(SaveManager::SaveBlock* block) {
    Reset();
    block->SkipToChunk(SaveManager::kStateBase);
    {
        SaveManager::Reader r(block->GetChunk());
        r.u32();   // version
        playerSeed = r.u32();
        SetResourceAmountValidated(kLevel, r.u32());
        int n = r.u8();
        for (int i = 0; i < n; ++i) {
            SetResourceAmountValidated(i, r.u32());
            g_earned[i] = r.u32();
            g_spent[i] = r.u32();
        }
        uint32_t count = r.u32();
        for (uint32_t i = 0; i < count; ++i) g_completedTasks.insert(r.u32());
        LoadMap(r, g_subTasks);
        count = r.u32();
        for (uint32_t i = 0; i < count; ++i) {   // obsolete
            r.u32();
            r.u32();
            r.u8();
        }
        block->EndChunkLoading(r);
    }
    bool wolfQuestFix = true;
    tutorial = 0x80;
    lastSentStep = 0x100;
    secondTutorial = 0x100;
    g_timeStart = Timer::GetGlobalTime();
    for (;;) {
        uint32_t type = block->NextChunkID();
        if (type == SaveManager::kEnd || type == SaveManager::kEntityHeader) break;
        if (type == 8) {   // PresentsContainer::Load
            block->SkipToChunk(8);
            g_presents = block->GetChunk().data;
            continue;
        }
        if (type == 0x12 || type == 0x2a || type == 0x37) {
            block->SkipChunk(true);
            continue;
        }
        bool known = (type >= 0xf && type <= 0x11) || type == 0x15 || type == 0x16 ||
                     (type >= 0x18 && type <= 0x22) || type == 0x24 || type == 0x26 || type == 0x27 ||
                     type == 0x29 || (type >= 0x2b && type <= 0x2e && type != 0x2d) ||
                     (type >= 0x34 && type <= 0x4f && type != 0x37 && type != 0x42 && type != 0x44 && type != 0x45) ||
                     (type >= 0x51 && type <= 0x59 && type != 0x53) || type == 0x60 || type == 0x66 || type == 0x68;
        if (!known) {
            std::fprintf(stderr, "GameState::Load() Unknown save chunk %d\n", type);
            block->SkipChunk(false);
            continue;
        }
        SaveManager::Reader r(block->GetChunk());
        switch (type) {
        case 0xf:
            Timer::ResetTimeAdvance();
            Timer::AdvanceGlobalTime(r.u32());
            break;
        case 0x10: {
            g_male = r.u32() != 0;
            g_playerName = LoadString(r);
            g_castleName = LoadString(r);
            break;
        }
        case 0x11: {   // CHUNK_STATE_PLAYER_ITEMS (the first format)
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t id = r.u32();
                int amount = (int)r.u32();
                AddItem(id, amount, false, false, false);
            }
            n = r.u32();
            for (uint32_t i = 0; i < n && i < 10; ++i) g_itemBindings[i] = ItemBinding{r.u32(), 0, nullptr};
            break;
        }
        case 0x26:     // CHUNK_STATE_PLAYER_ITEMS_V2
        case 0x3a: {   // CHUNK_STATE_PLAYER_ITEMS_V3
            g_itemUniqueId = 1;
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) {
                auto it = std::make_unique<PlayerItem>();
                it->id = r.u32();
                it->uniqueId = r.u32();
                it->f0c = r.u32();
                it->f10 = r.u32();
                bool f14 = type == 0x3a && r.u32() != 0;
                it->info = Items::GetItemInfo(it->id);
                if (type == 0x26) {
                    if (!it->info) continue;   // (version 2 keeps only known items)
                } else {
                    if (it->f10 == 0 && it->info && it->info->durability != 0)
                        it->f0c = it->f10 = it->info->durability;
                    it->f14 = f14;
                }
                if (g_itemUniqueId < it->uniqueId) g_itemUniqueId = it->uniqueId;
                g_items.push_back(std::move(it));
            }
            n = r.u32();
            for (uint32_t i = 0; i < n && i < 10; ++i) g_itemBindings[i] = ItemBinding{0, r.u32(), nullptr};
            break;
        }
        case 0x15: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n && i < 3; ++i) g_belt[i] = r.u32();
            break;
        }
        case 0x16: g_mapId = r.u32(); break;
        case 0x18: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) g_startedTasks.insert(r.u32());
            break;
        }
        case 0x19: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) {
                int key = (int)r.u32();
                uint32_t k = r.u32();
                for (uint32_t j = 0; j < k; ++j) {
                    uint32_t a = r.u32();
                    uint32_t b = r.u32();
                    g_bossFights[key].push_back({a, b});
                }
            }
            break;
        }
        case 0x1a: LoadMap(r, g_mapCompletion); break;
        case 0x1b: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n && i < 3; ++i) g_beltActivation[i] = r.u32();
            break;
        }
        case 0x1c: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n && i < 3; ++i) g_beltOld[i] = r.u32();
            break;
        }
        case 0x1d: LoadMap(r, g_mapStartTime); break;
        case 0x1e: tutorial = lastSentStep = (int)r.u32(); break;
        case 0x1f: {
            mHPTS = r.u32();
            if ((uint32_t)Timer::GetGlobalTime() < mHPTS) mHPTS = (uint32_t)Timer::GetGlobalTime();
            break;
        }
        case 0x20: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) {
                OfflineBuilding b;
                b.id = r.u32();
                b.level = r.u32();
                b.contract = r.u32();
                b.flags = r.u32();
                if (b.flags & 0xffffff00) b.flags = 0;
                b.stateTime = r.u32();
                b.f14 = r.u8() != 0;
                b.f15 = r.u8() != 0;
                b.x = r.u32();
                b.y = r.u32();
                g_offlineBuildings.push_back(b);
            }
            break;
        }
        case 0x21: LoadMap(r, g_taskShowTime); break;
        case 0x22: LoadMap(r, g_mapCollectionInfo); break;
        case 0x24: lastSentStep = secondTutorial = (int)r.u32(); break;
        case 0x27: LoadMap(r, g_mapEndTime); break;
        case 0x29: {   // SoldierPool::SetFreeSlotsCount
            uint32_t v = r.u32();
            int max = Setting("reserve_slot_max").GetInt();
            if (max < (int)v) v = (uint32_t)Setting("reserve_slot_max").GetInt();
            SoldierPool::SetFreeSlotsCount(v);
            break;
        }
        case 0x2b: g_timeStart = r.u32(); break;
        case 0x2c: LoadMap(r, g_itemAcquireTime); break;
        case 0x2e: LoadMap(r, g_itemRecentTime); break;
        case 0x34: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) {
                int k = (int)r.u32();
                uint32_t v = r.u32();
                g_bossFightsTimers.insert({k, v});
            }
            break;
        }
        case 0x35: LoadMap(r, g_taskBeginTime); break;
        case 0x36:
            g_dailyBonus[0] = r.u32();
            g_dailyBonus[1] = r.u32();
            break;
        case 0x38: LoadMap(r, g_campaignCompletion); break;
        case 0x39: LoadMap(r, g_taskEndTime); break;
        case 0x3b:
            wolfQuestFix = false;
            r.u32();
            break;
        case 0x3c:
            g_socnetConnections[0] = r.u32() != 0;
            g_socnetConnections[1] = r.u32() != 0;
            break;
        case 0x3d: g_tutorialType = r.u32(); break;
        case 0x3e:
            g_expansionVersions[0] = r.u32();
            g_expansionVersions[1] = r.u32();
            break;
        case 0x3f: g_mainExpansionBackgroundLoad = r.u32(); break;
        case 0x40: g_dailyBonusCount = r.u32(); break;
        case 0x41: LoadMap(r, g_professionPoints); break;
        case 0x43: g_resourceMismatches = (int)r.u32(); break;
        case 0x46: LoadMap(r, g_eventStartTime); break;
        case 0x47: LoadMap(r, g_eventEndTime); break;
        case 0x48: LoadMap(r, g_mapVersion); break;
        case 0x49: LoadMap(r, g_eventDifficulty); break;
        case 0x4a: LoadMap(r, g_eventCompletion); break;
        case 0x4b: LoadMap(r, g_monsterKills); break;
        case 0x4c:
            for (U32Map& m : g_arenaStats) LoadMap(r, m);
            break;
        case 0x4d: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t k = r.u32();
                g_warriorRespawn[k].first = r.u32();
                g_warriorRespawn[k].second = r.u32();
            }
            break;
        }
        case 0x4e: {   // Neighbours
            int len = r.s16();
            for (int i = 0; i < 0x7f; ++i) g_neighbourSaveName[i] = (char32_t)(int)(int16_t)((uint16_t)r.s16() ^ 0xaaaa);
            if (len > 0x7e) len = 0x7f;
            g_neighbourSaveName[len] = 0;
            g_questFriendCount = r.u32();
            break;
        }
        case 0x4f: {   // the online stats: only the PvP counts are read back
            r.u32();
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i)
                if (r.u32() != 0) { r.u32(); r.u32(); r.u32(); }
            n = r.u32();
            for (uint32_t i = 0; i < n; ++i)
                if (r.u32() != 0) r.u32();
            n = r.u32();
            for (uint32_t i = 0; i < n; ++i) { r.s16(); r.s16(); r.u8(); r.s16(); r.s16(); }
            g_pvpVictories = r.u32();
            g_pvpDefeats = r.u32();
            n = r.u32();
            if (n < 0x400 && r.HasData((size_t)n * 4))
                for (uint32_t i = 0; i < n; ++i) r.u32();
            break;
        }
        case 0x51: LoadMap(r, g_customSubtaskRequirement); break;
        case 0x52:
            LoadMap(r, g_soldierAcquireTime);
            LoadMap(r, g_soldierRecentTime);
            LoadMap(r, g_soldierAcquireCount);
            break;
        case 0x54: {
            uint32_t n = r.u32();
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t v = r.u32();
                if (i < 0x20) g_customBindings[i].id = v;
            }
            break;
        }
        case 0x55: LoadMap(r, g_mapVisitTime); break;
        case 0x56: {   // PvP tutorial
            g_pvpTutorialLastRefresh = r.u32();
            g_pvpTutorialCompleted = r.u8() != 0;
            uint32_t n = r.u32();
            g_pvpTutorialPlayers.clear();
            for (uint32_t i = 0; i < n; ++i) {
                PvPTutorialPlayer pl;
                pl.a = r.u32();
                pl.b = r.u8();
                pl.c = r.u8();
                g_pvpTutorialPlayers.push_back(pl);
            }
            break;
        }
        case 0x57: LoadMap(r, g_itemBuff); break;
        case 0x58:
            LoadMap(r, g_chestItemReceived);
            LoadMap(r, g_chestBuildingReceived);
            LoadMap(r, g_chestEntityReceived);
            break;
        case 0x59: LoadMap(r, g_featuredTaskClickTime); break;
        case 0x60: LoadMap(r, g_purchaseActive); break;
        case 0x66: {
            uint8_t key[32];
            SidKey(key);
            uint32_t n = r.u32();
            g_sid.clear();
            for (uint32_t i = 0; i < n; ++i) g_sid += (char)(key[i & 0x1f] ^ r.u8());
            g_hasSid = true;
            n = r.u32();
            g_guid.clear();
            for (uint32_t i = 0; i < n; ++i) g_guid += (char)(key[i & 0x1f] ^ r.u8());
            g_hasGuid = true;
            break;
        }
        case 0x68: g_playerType = r.u32(); break;   // PlayerProfileManager::SetPlayerType
        }
        block->EndChunkLoading(r);
    }
    // The bindings saved by id (old saves, the customization) take the first such item; those
    // saved by unique id find theirs.
    for (ItemBinding* list : {g_itemBindings, g_customBindings}) {
        size_t n = list == g_itemBindings ? 10 : 32;
        for (size_t i = 0; i < n; ++i) {
            ItemBinding& b = list[i];
            if (b.id != 0) {
                b.item = GetFirstItemByID(b.id, false);
                b.uniqueId = b.item ? b.item->uniqueId : 0;
                b.id = 0;
            } else {
                b.item = GetItemByUniqueID(b.uniqueId);
                if (!b.item) b.uniqueId = 0;
            }
        }
    }
    for (uint32_t i = GetBeltSlotCount(); i < 3; ++i) g_belt[i] = g_beltActivation[i] = g_beltOld[i] = 0;
    if (wolfQuestFix && GetTaskBeginTime(0x1b8) && !TaskCompleted(0x1b8) && GetItemAmount(0x16e, false) == 3) {
        AddItem(0x16e, 1, false, false, false);
        // UNVERIFIED (milestone 4c): Tasks::CompleteSubtaskDirect(0x323, 1).
    }
}

void SaveEntities() {
    BeginChunk(SaveManager::kEntityHeader);
    SaveShort((int16_t)EntityManager::GetEntityCount(true));
    EndChunk();
    uint32_t nextId = 0;
    for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i)
        if (nextId < e->GetUniqueID()) nextId = e->GetUniqueID();
    ++nextId;
    for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i) {
        if (e->temporary) continue;
        BeginChunk(SaveManager::kEntity);
        SaveShort((int16_t)e->GetEntityData()->id);
        SaveChar(e->hasHome);
        SaveShort((int16_t)(e->hasHome ? e->homeX : 0));
        SaveShort((int16_t)(e->hasHome ? e->homeY : 0));
        SaveChar((uint8_t)(e->workType > 0 ? e->workType : 0));
        SaveShort((int16_t)(e->workType > 0 ? e->workX : 0));
        SaveShort((int16_t)(e->workType > 0 ? e->workY : 0));
        SaveUnsigned((uint32_t)e->f78);
        SaveShort((int16_t)(e->GetHpOverlimit() + e->GetHP()));
        SaveChar((uint8_t)e->firstNameIdx);
        SaveChar((uint8_t)e->surnameIdx);
        SaveShort((int16_t)e->GetCurrentMap());
        SaveChar(e->f3b);
        EndChunk();
        BeginChunk(0x23);
        SaveShort((int16_t)e->GetAP());
        EndChunk();
        BeginChunk(0x32);
        SaveShort((int16_t)e->GetHpMax());
        EndChunk();
        BeginChunk(0x53);
        SaveShort((int16_t)e->GetOverrideAttack());
        SaveShort((int16_t)e->GetOverrideDefense());
        EndChunk();
        BeginChunk(0x67);
        SaveUnsigned(e->GetUniqueID() ? e->GetUniqueID() : nextId++);
        EndChunk();
    }
}

// The player's city: the player's hero (0xc/0xd; a second one is an error) with its squad, the
// soldiers (in SoldierSlots and the hero's squad when +0x3b is set, else inactive in SoldierPool; dead
// ones that are not elite are left to EntityManager::Clean), workers and farmers. In the player's city
// the loaded chunks are also copied to entityDataCopy (LoadEntitiesFromCopy restores them on the
// return from a campaign map); a dropped soldier's follower chunks are not copied, as on the original.
void LoadEntities(SaveManager::SaveBlock* block) {
    std::unique_ptr<SaveManager::SaveBlock> copy;
    if (IsPlayerCity()) copy = std::make_unique<SaveManager::SaveBlock>();
    block->SkipToChunk(SaveManager::kEntityHeader);
    const SaveManager::Chunk& header = block->GetChunk();
    if (copy) copy->AddChunk(header);
    SaveManager::Reader hr(header);
    int count = hr.s16();
    block->EndChunkLoading(hr);
    bool havePlayer = false;
    for (int i = 0; i < count; ++i) {
        block->SkipToChunk(SaveManager::kEntity);
        const SaveManager::Chunk& c = block->GetChunk();
        if (copy) copy->AddChunk(c);
        SaveManager::Reader r(c);
        int id = r.s16();
        bool hero = (unsigned)(id - 0xc) <= 1;
        Entity* e;
        if (IsPlayerCity()) {
            if (hero && havePlayer) {
                std::fprintf(stderr, "ERROR: Save contains multiple player entities (ID: %d)", id);
                block->EndChunkLoading(r);
                continue;
            }
            e = EntityManager::CreateEntity(id, false, true);
            if (hero) havePlayer = true;
        } else {
            e = EntityManager::CreateEntity(id, hero, true);
        }
        e->temporary = false;
        // PORT: a visited friend's hero (class 5 outside the player's city) becomes class 0x16 on the
        // original (EntityData::PartialClone, SetCustomEntityData); friends' cities are online only.
        e->hasHome = r.u8() != 0;
        e->homeX = r.s16();
        e->homeY = r.s16();
        e->workType = r.u8();
        e->workX = r.s16();
        e->workY = r.s16();
        e->f78 = (int)r.u32();
        e->SetHP(r.s16());
        e->firstNameIdx = r.u8();
        e->surnameIdx = r.u8();
        int clas = e->GetEntityData()->clas;
        if (clas == 0 || clas == 2) {   // workers, farmers
            int farmer = clas == 2 ? 1 : 0;
            e->firstName = EntityFactory::GetNameByIdx(e->GetEntityData()->female, farmer, e->firstNameIdx);
            e->surname = EntityFactory::GetSurnameByIdx(e->GetEntityData()->female, farmer, e->surnameIdx);
        }
        e->SetCurrentMap(r.s16());
        e->f3b = r.u8() != 0;
        if (clas == 5 || clas == 0x16) e->CreateSquad();
        if (clas == 10 && e->IsDead() && !e->IsEliteSoldier()) {
            e->temporary = true;
            block->EndChunkLoading(r);
            continue;
        }
        if (!e->f3b) {
            e->SetActive(false, false);
            if (clas == 10 && IsPlayerCity()) SoldierPool::AddSoldier(e);
        } else if (!IsPlayerCity()) {
            GetFriendPlayer()->GetSquad()->AddSoldier(e);
        } else {
            EntityManager::GetPlayer()->GetSquad()->AddSoldier(e);
            SoldierSlots::AddSoldier(e);
        }
        block->EndChunkLoading(r);
        if (block->NextChunkID() == 0x23) {
            const SaveManager::Chunk& f = block->GetChunk();
            if (copy) copy->AddChunk(f);
            SaveManager::Reader fr(f);
            e->SetAP(fr.s16());
            block->EndChunkLoading(fr);
        }
        if (block->NextChunkID() == 0x32) {
            const SaveManager::Chunk& f = block->GetChunk();
            if (copy) copy->AddChunk(f);
            SaveManager::Reader fr(f);
            e->SetHpMax(fr.s16());
            block->EndChunkLoading(fr);
        }
        if (block->NextChunkID() == 0x53) {
            const SaveManager::Chunk& f = block->GetChunk();
            if (copy) copy->AddChunk(f);
            SaveManager::Reader fr(f);
            e->SetOverrideAttack(fr.s16());
            e->SetOverrideDefense(fr.s16());
            block->EndChunkLoading(fr);
        }
        if (block->NextChunkID() == 0x67) {
            SaveManager::Reader fr(block->GetChunk());
            e->SetUniqueID(fr.u32());
            block->EndChunkLoading(fr);
        }
        if (e->player) {
            // The saved HP may be above HpMax: the excess becomes the HP over-limit.
            Entity::SetupPlayerHPOverlimit(true, true);
            e->AddHP(0);
            Entity::SetupPlayerHPOverlimit(false, true);
        }
    }
    if (copy) {
        copy->BeginChunk(SaveManager::kEnd);
        SaveManager::PutUnsigned(copy->cur.data, 0);
        copy->EndChunk();
        copy->MergeChunks();
        g_entityDataCopy = std::move(copy);   // CopyEntityData
    }
}

unsigned GetSoldierSlotCount() {
    return TaskCompleted((unsigned)Setting("squad_slot_02_quest_unlock").GetInt()) ? 3 : 2;
}

Entity* GetFriendPlayer() {
    for (unsigned i = 0; Entity* e = EntityManager::EnumEntities(i); ++i)
        if (e->GetEntityData()->clas == 0x16) return e;
    return nullptr;
}

void SetSoldierReserveSlots(unsigned n) { SoldierPool::SetFreeSlotsCount(n); }
unsigned GetSoldierReserveSlots() { return SoldierPool::GetTotalSlotsFree(); }

BaseCombat* GetActiveCombat() { return nullptr; }   // UNVERIFIED (milestone 4g): currentCombat

}  // namespace GameState
