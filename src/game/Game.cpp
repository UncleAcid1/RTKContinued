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

bool done = false;
bool paused = false;
float pauseTime = 0.f;

void MainExit() { done = true; }

void ExitWithSendSave() { MainExit(); }

// (The listMaps debug string of the visited maps missing from the save, the expansion pack
// checks, SoldierSlots, the chapters, FileManager::SetGameState, the online player data and
// HUDWindow::UpdateTasks are not ported; main shows the HUD.)
bool LoadSavedGame(bool listMaps) {
    (void)listMaps;
    SaveManager::SaveBlock* state = SaveManager::GetGameStateData();
    if (!state) return false;
    GameState::Load(state);
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
