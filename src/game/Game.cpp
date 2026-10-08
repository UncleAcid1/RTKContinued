#include "game/Game.h"

#include <cstdio>

#include "engine/FileManager.h"
#include "engine/SystemFuncs.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/GameState.h"
#include "game/Map.h"
#include "game/SaveManager.h"
#include "game/SoldierSlots.h"
#include "game/Tags.h"

bool done = false;
bool paused = false;
float pauseTime = 0.f;

void MainExit() { done = true; }

void ExitWithSendSave() { MainExit(); }

// (The listMaps debug string of the visited maps missing from the save, the expansion pack
// checks, the chapters, FileManager::SetGameState, the online player data and
// HUDWindow::UpdateTasks are not ported; main shows the HUD.)
bool LoadSavedGame(bool listMaps) {
    (void)listMaps;
    SaveManager::SaveBlock* state = SaveManager::GetGameStateData();
    // UNVERIFIED (milestone 4h): without a save the original starts a new game here (the hero, its
    // squad and soldier 0x1a2 on campaign map 0x15); main's PORT test path does the city instead.
    if (!state) return false;
    GameState::Load(state);
    SoldierSlots::SetFreeSlotsCount(GameState::GetSoldierSlotCount());
    SaveManager::SaveBlock* city = SaveManager::GetMapData(0);
    GameState::LoadEntities(state);
    GameState::SetCurrentMapID(0);
    Map::Load(city, (uint32_t)Timer::GetGlobalTime());
    Map::UpdateOfflineResources();
    return true;
}

void SetSaveNames() {
    std::string last = SystemFuncs::GetSetting_String("last_played_save", "");
    bool plain = last.empty() || (!FileManager::SaveExists("save_offline.bin") && !FileManager::SaveExists("save_fb.bin"));
    if (plain) {
        SaveManager::SetCurrentSaveName("save.bin");
        SaveManager::SetBackupSaveName("save.binprev");
        return;
    }
    last = SystemFuncs::GetSetting_String("last_played_save", "offline");
    SaveManager::SetCurrentSaveName("save_" + last + ".bin");
    SaveManager::SetBackupSaveName("save_" + last + ".binprev");
}

void SaveOnExit() {
    BuildingHovers::CollectAll();
    BuildingMovement::Decline();
    BuildingPlacement::Decline();
    // UNVERIFIED (milestone 4): SpellMovement::Decline.
    BuildingMovement::Deinit();
    BuildingPlacement::Deinit();
    // UNVERIFIED (milestone 4): SpellMovement::Deinit, MapMovement::Deinit.
    if (GameState::IsCityTutorial()) return;
    Map::Save(0);
}

void UpdateTagState() {
    static uint32_t s_lastTime = 0;   // 0x6122e8
    uint32_t now = (uint32_t)Timer::GetGlobalTime();
    if (now == s_lastTime) return;
    s_lastTime = now;
    bool changed = false;
    for (unsigned i = 0;; ++i) {
        Tags::Tag* t = Tags::EnumTag(i);
        if (!t) break;
        if (t->f05 == t->IsActive()) continue;
        changed = true;
        // UNVERIFIED (milestone 4c): EntityManager::OnTagUpdate (0x16664c): entities whose meta
        // expression (type 0x99) names the tag appear or are set up again.
        t->f05 = t->IsActive();
    }
    if (!changed) return;
    // UNVERIFIED (milestone 4d): HUDWindow::UpdateTasks (0x2c7ba8).
}
