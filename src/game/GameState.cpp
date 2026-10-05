#include "game/GameState.h"

#include <map>
#include <string>

#include "game/StringTable.h"

namespace GameState {
namespace {
std::map<int, uint32_t> g_res;
int g_cityState = 0;        // 0/1: player's city (IsPlayerCity), 2+: visiting
int g_location = 0;
uint32_t g_mapId = 0;
int g_pause = 0;
std::u32string g_castleName;
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
