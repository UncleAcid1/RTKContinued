#include "game/GameState.h"

#include <cstdio>
#include <strings.h>
#include <map>
#include <string>

#include "game/Setting.h"
#include "game/StringTable.h"

namespace GameState {

bool updated = true;             // 0x60efcc (a change to save)
int resourceAmountMax = 0;       // 0x6134a4 storage limit (CheckStorageFull)
int maxLevel = 0x1d;             // 0x60efc8
int totalGoldSpent = 0;          // 0x613418
int totalGoldEarned = 0;         // 0x61341c
int totalCrystalsSpent = 0;      // 0x613420
int totalXPEarned = 0;           // 0x613424

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
uint32_t g_mapId = 0;
int g_pause = 0;
std::u32string g_castleName;
}

// UNVERIFIED (milestone 3): only the resource part of Reset @0x1a1424 is ported so far.
void Reset() {
    g_res[0xdd] = 0x84358e6d;
    g_res[0xee] = 0x14a46840;
    SetResourceAmountValidated(kLevel, 1);
    for (int i = 0; i < 0xb; ++i) SetResourceAmountValidated(i, 0);
    SetResourceAmountValidated(kExpAfter, 1);
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
            // UNVERIFIED (milestone 4 / 3e): Tasks::CompleteSubtask(7, 0, 1) and LevelUpWindow::Show().
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
uint32_t GetCurrentMapID() { return g_mapId; }
void SetCurrentMapID(uint32_t id) {
    g_location = id != 0 ? 2 : 0;   // SetCurrentLocation: campaign maps / city
    g_mapId = id;
}
bool IsTutorial() { return false; }
int GetPlayerWorkersCount() { return 0; }
int GetMaxWorkerCount() { return 0; }
bool IsMalePlayer() { return true; }

const char32_t* GetCastleName() {
    if (!g_castleName.empty()) return g_castleName.c_str();
    return StringTable::GetString("world_node_player_no_name");
}
void SetCastleName(const char32_t* name) { g_castleName = name; }
int TutorialStep() { return 0x100; }
int SecondTutorialStep() { return 0x81; }

bool IsCityTutorial() {
    if (TutorialStep() <= 0x67) return true;
    unsigned second = (unsigned)SecondTutorialStep();
    if (second - 0x8au <= 0x75u) return true;
    return second - 0x82u < 7u;
}

void RaiseGamePauseState() { ++g_pause; }
void DropGamePauseState() {
    if (--g_pause < 0) g_pause = 0;
}
bool IsPaused() { return g_pause > 0; }

}  // namespace GameState
