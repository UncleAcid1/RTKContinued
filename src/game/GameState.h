// GameState: the player's state. MILESTONE 2 STAND-IN: only what the HUD reads, holding the values of
// a freshly reset game (GameState::Reset @0x1a1424 sets level (0xbb) = 1, resources 0..9 = 0,
// EXP_AFTER (10) = 1). The original stores every value three times XOR-scrambled (anti-cheat,
// GetResourceAmount @0x196e08); that storage, saves and the economy are milestone 3.
#pragma once
#include <cstdint>

namespace GameState {

enum ResourceType {   // GameState::GetResourceName table @0x601568
    kLumber = 0, kRocks = 1, kFood = 2, kPlanks = 3, kStones = 4, kMeat = 5, kSausage = 6, kOil = 7,
    kGold = 8, kCrystal = 9, kExpAfter = 10, kLevel = 0xbb
};

void Reset();
uint32_t GetResourceAmount(int type);          // @0x196e08
void SetResourceAmount(int type, uint32_t v);
int GetLevel();                                // @0x196e00 (resource 0xbb)
const char* GetResourceName(int type);         // @0x190ce4

bool IsPlayerCity();                           // @0x1905b8: city state 0 or 1
int GetCurrentLocation();                      // @0x190530: 0 city, 1 farm, 2 campaign, 3 arena
uint32_t GetCurrentMapID();                    // @0x190558
void SetCurrentMapID(uint32_t id);             // @0x19056c
bool IsTutorial();                             // UNVERIFIED stand-in: false
int GetPlayerWorkersCount();                   // UNVERIFIED stand-in: 0
int GetMaxWorkerCount();                       // UNVERIFIED stand-in: 0
bool IsMalePlayer();                           // UNVERIFIED stand-in: true
// The player's castle name; without one, StringTable "world_node_player_no_name".
const char32_t* GetCastleName();               // @0x191d98
void SetCastleName(const char32_t* name);      // @0x191e34
// TutorialWindow's step (global 0x6134ac). HUD panels appear once it passes their thresholds;
// 0x100 is the finished city tutorial. UNVERIFIED stand-in: always 0x100.
int TutorialStep();
// GameState::secondTutorial (0x60efd0, .data initial 0x81). UNVERIFIED stand-in: the initial value.
int SecondTutorialStep();
bool IsCityTutorial();                         // @0x190af4

// The pause counter (0x612d70): dialogs raise it while shown.
void RaiseGamePauseState();                    // @0x191064
void DropGamePauseState();                     // @0x191080 (never below 0)
bool IsPaused();                               // @0x191104

}  // namespace GameState
