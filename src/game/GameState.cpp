#include "game/GameState.h"

#include <map>

namespace GameState {
namespace {
std::map<int, uint32_t> g_res;
int g_cityState = 0;        // 0/1: player's city (IsPlayerCity), 2+: visiting
int g_location = 0;
uint32_t g_mapId = 0;
}

void Reset() {
    g_res.clear();
    g_res[kLevel] = 1;
    for (int i = 0; i < 10; ++i) g_res[i] = 0;
    g_res[kExpAfter] = 1;
}

uint32_t GetResourceAmount(int type) {
    auto it = g_res.find(type);
    return it == g_res.end() ? (type == kLevel ? 1u : 0u) : it->second;
}
void SetResourceAmount(int type, uint32_t v) { g_res[type] = v; }
int GetLevel() { return (int)GetResourceAmount(kLevel); }

const char* GetResourceName(int type) {
    static const char* names[] = {"LUMBER", "ROCKS", "FOOD", "PLANKS", "STONES", "MEAT", "SAUSAGE",
                                  "OIL", "GOLD", "CRYSTAL", "EXP_AFTER"};
    return type >= 0 && type < 11 ? names[type] : "";
}

bool IsPlayerCity() { return (unsigned)g_cityState <= 1; }
int GetCurrentLocation() { return g_location; }
uint32_t GetCurrentMapID() { return g_mapId; }
bool IsTutorial() { return false; }
int GetPlayerWorkersCount() { return 0; }
int GetMaxWorkerCount() { return 0; }
bool IsMalePlayer() { return true; }
int TutorialStep() { return 0x100; }

}  // namespace GameState
