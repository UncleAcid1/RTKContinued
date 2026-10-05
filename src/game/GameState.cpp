#include "game/GameState.h"

#include <map>
#include <string>

#include "game/StringTable.h"

namespace GameState {
namespace {
// std::map<ResourceType,int> (tree header 0x612f78): every value v of type t is kept three times, as [t] = v,
// [t+0x2000] = v ^ [0xdd] and [t+0x4000] = v ^ [0xee], with the two keys stored in the same map.
std::map<int, uint32_t> g_res;
int g_resourceMismatches = 0;   // 0x612d60
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
