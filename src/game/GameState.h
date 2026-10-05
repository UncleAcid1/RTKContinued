// GameState: the player's state (resources, level, settings, tutorial, ...). Being ported in
// milestone 3; the settings and areas are in game/Setting.h.
#pragma once
#include <cstdint>

namespace GameState {

enum ResourceType {   // GameState::GetResourceName table @0x601568
    kLumber = 0, kRocks = 1, kFood = 2, kPlanks = 3, kStones = 4, kMeat = 5, kSausage = 6, kOil = 7,
    kGold = 8, kCrystal = 9, kExpAfter = 10, kLevel = 0xbb
};

extern bool updated;             // a change waiting to be saved
extern int resourceAmountMax;    // storage limit
extern int maxLevel;
extern int totalGoldSpent, totalGoldEarned, totalCrystalsSpent, totalXPEarned;

void Reset();
uint32_t GetResourceAmount(int type);          // @0x196e08
void SetResourceAmountValidated(int type, uint32_t v);   // @0x196ed0
uint32_t GetResourceAmountValidated(int type);   // @0x196c78 (same as GetResourceAmount)
int GetLevel();                                // @0x196e00 (resource 0xbb)
bool CheckStorageFull(int type);               // @0x196dd8
// @0x197018: add (or spend, negative) a resource; XP raises the level. Crystals only go down here.
void ChangeResourceAmount(int type, int amount);
void AddCrystals(int amount);                  // @0x196f68
const char* GetResourceName(int type);         // @0x190ce4
int StringToResourceType(const char* name);    // @0x192580 (case-insensitive; unknown -> GOLD)

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
