// NotEnoughWindow: what a purchase lacks, and the dialog offering it for crystals. Port of
// NotEnoughWindow (libkingdom.so 5.11). The requirements are ported; the dialog comes with 3e.4.
#include "windows/Windows.h"

#include <cstdio>
#include <utility>
#include <vector>

#include "game/GameState.h"
#include "game/Map.h"

namespace NotEnoughWindow {
namespace {

unsigned g_required[11] = {};   // 0x625924 per resource type
unsigned g_level = 0;           // 0x625950
unsigned g_population = 0;      // 0x625968
std::vector<std::pair<unsigned, unsigned>> g_items;          // 0x62595c (item id, count)
std::vector<std::pair<unsigned, unsigned>> g_buildingCounts; // 0x625974 (building id, count)
std::vector<std::pair<unsigned, unsigned>> g_buildingLevels; // 0x625980 (building id, level)
unsigned g_exchangeLimit = 0;   // 0x62598c
int g_missing = 0;              // 0x625d6c

}  // namespace

void ResetRequirements() {
    for (unsigned& r : g_required) r = 0;
    g_level = 0;
    g_population = 0;
    g_items.clear();
    g_buildingCounts.clear();
    g_buildingLevels.clear();
    g_exchangeLimit = 0;
    // UNVERIFIED (3e.4): also clears the reputation (0x625958), the upgradable building and item to
    // produce, and the dialog's action callback.
}

void AddRequirement(int type, unsigned amount) { g_required[type] = amount; }
void AddLevelRequirement(unsigned level) { g_level = level; }
void AddPopulaionRequirement(unsigned people) { g_population = people; }
void AddBuildingLevelRequirement(unsigned id, unsigned level) { g_buildingLevels.emplace_back(id, level); }
void SetExchangeLimit(unsigned limit) { g_exchangeLimit = limit; }

bool CheckRequirements() {
    g_missing = 0;
    for (int i = 0; i < 11; ++i) {
        int need = (int)g_required[i];
        if (need != 0 && (int)GameState::GetResourceAmount(i) < need) ++g_missing;
    }
    if (g_level != 0 && (unsigned)GameState::GetLevel() < g_level) ++g_missing;
    // UNVERIFIED (milestone 4): the profession level requirement (Professions) is not ported.
    for (auto& [id, n] : g_items)
        if ((unsigned)GameState::GetItemAmount(id, false) < n) ++g_missing;
    if (g_population != 0 &&
        GameState::GetPlayerWorkersCount() - Map::GetUsedWorkerCount() < (int)g_population)
        ++g_missing;
    for (auto& [id, n] : g_buildingCounts)
        if ((unsigned)Map::GetBuildingCount(id, false) < n) ++g_missing;
    for (auto& [id, n] : g_buildingLevels)
        if ((unsigned)Map::GetBuildingMaxUpgrade(id) < n) ++g_missing;
    return g_missing == 0;
}

void SetDescriptionText(const char32_t*, const char32_t*) {}
void SetActionCallback(std::function<void()>, const char32_t*, bool) {}
void Hide() {}

void Show() {
    for (int i = 0; i < 11; ++i) {
        if (g_required[i] != 0 && (int)GameState::GetResourceAmount(i) < (int)g_required[i])
            std::printf("NotEnoughWindow: %s %u needed, %u held\n", GameState::GetResourceName(i), g_required[i],
                        GameState::GetResourceAmount(i));
    }
    if (g_level != 0 && (unsigned)GameState::GetLevel() < g_level) std::printf("NotEnoughWindow: level %u needed\n", g_level);
    if (g_population != 0 && GameState::GetPlayerWorkersCount() - Map::GetUsedWorkerCount() < (int)g_population)
        std::printf("NotEnoughWindow: %u people needed\n", g_population);
}

}  // namespace NotEnoughWindow
