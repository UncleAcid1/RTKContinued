#include "hud/HUD.h"
#include "game/Game.h"

#include <cstdio>

#include "engine/FileManager.h"
#include "engine/IconManager.h"
#include "engine/SystemFuncs.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/GameState.h"
#include "game/EntityManager.h"
#include "game/Map.h"
#include "game/MetaData.h"
#include "game/SaveManager.h"
#include "game/SoldierSlots.h"
#include "game/Squad.h"
#include "game/StringTable.h"
#include "game/Tags.h"
#include "gui/WindowManager.h"
#include "windows/Windows.h"

bool done = false;
bool gameLoaded = false;
bool forceExitNoSave = false, restartGameAfterExit = false;
bool canPlayMusic = false;
std::string gMusicName;
unsigned languageID = 0, availableLanguageID = 0;
bool languageHasBeenSelected = false;
bool paused = false;
bool renderSuspendEvent = false;   // 0x6123fc
float pauseTime = 0.f;

void MainExit() { done = true; }

// @0x184f04 MainExitNoSave
void MainExitNoSave() {
    forceExitNoSave = true;
    done = true;
    restartGameAfterExit = true;
}

// @0x16aeb0 FileManager::SetPreferredLanguade
void FileManager::SetPreferredLanguade(unsigned id) {
    SystemFuncs::SetSetting_Int("selected_language", (int)id, "MyCountry_flags");
}

// @0x16b0a4 FileManager::RestartGame
void FileManager::RestartGame() {
    forceExitNoSave = true;
    done = true;
    restartGameAfterExit = true;
}

// @0x185668 OnResume
void OnResume() {
    // (PlayerProfileManager::ResetNoConnectionWarning: online. PlayerWeakWindow::ResetSession:
    // milestone 5.)
    // UNVERIFIED (milestone 5): CastleTopWindow::ActivateFeatureHolders (0x28a53c).
    // UNVERIFIED (milestone 5, the daily bonus): with no bonus received yet today
    // (GameState::GetReceivedDailyBonusCount), task 0x35c completed, the connection not being
    // checked (temporary setting 2), no window, combat, hover, world dialog or pause:
    // DailyBonusWindow::ForceBonus and Show.
}

// @0x187674 turnMusic
void turnMusic(bool on) {
    // UNVERIFIED (milestone 6): SoundsManager::PauseAudio(!on) and, when on,
    // SoundsManager::PlayMusic(gMusicName, true).
    (void)on;
}

// @0x186fe0 LanguageDecline
void LanguageDecline() {
    PopupSelectionWindow::Hide();
    FileManager::SetPreferredLanguade(languageID);
    languageHasBeenSelected = true;
}

// @0x187060 LanguageSelect
void LanguageSelect() {
    PopupSelectionWindow::Hide();
    FileManager::SetPreferredLanguade(availableLanguageID);
    languageHasBeenSelected = true;
    Map::SafeSave();
    forceExitNoSave = true;
    FileManager::RestartGame();
    // (then SDL_Delay(1000): time for Android to act on it)
}

void AskLanguageQuestion() {
    if (languageHasBeenSelected) return;
    if (availableLanguageID == languageID || WindowManager::GetShownWindowCount() != 0 ||
        GameState::secondTutorial != 0x100 || GameState::IsCombatActive() || BuildingHovers::IsWorldDialogVisible() ||
        PlayerTopWindow::IsPlayerDialogVisible())
        return;
    std::u32string text = SWPrintf(0x100, StringTable::GetString("LANGUAGE_AVAILABLE"),
                                   {StringTable::GetStringF("LANGUAGE_%d", availableLanguageID)});
    PopupSelectionWindow::Show(StringTable::GetString("LANGUAGE_AVAILABLE_HEADER"), text.c_str(),
                               StringTable::GetString("HUD_OK"), IconManager::GetIcon("gold_confirm", false),
                               LanguageSelect, StringTable::GetString("CANCEL"),
                               IconManager::GetIcon("gold_cancel", false), LanguageDecline, nullptr);
    PopupSelectionWindow::SetMentorIcon();
}

namespace {
// The folder of the data files: res_files, or the sandbox server's res_files_sandbox
// (sandboxModeEnabled 0x6124a5, the "--sandbox" switch; online, never set by the port).
const char* const kDataFolder = "../resource/res_files/1Original/";

// The content packs' copies of a file ("active_packs": "name:number,..." in the settings, set by
// the expansion downloads): each pack whose expansion file exists (FileManager::
// DoesExpansionFileExist 0x16b4a8; online, dormant: none does) and which has the file gets it
// loaded on top.
template <typename F>
void LoadFromActivePacks(const char* file, F load) {
    std::string packs = SystemFuncs::GetSetting_String("active_packs", "");
    const char* p = packs.c_str();
    MetaData* list = ParseCustomStyleData(p, ",:", "111");
    if (!list) return;
    for (unsigned i = 0; i < list->GetChildrenCount(); ++i) {
        const MetaData* pack = list->GetChild(i);
        const bool expansionFileExists = false;   // (DoesExpansionFileExist(pack->GetChild(1)->GetInt()))
        if (!expansionFileExists) continue;
        std::string path = std::string(kDataFolder) + pack->GetChild(0)->GetString() + "/" + file;
        if (FileManager::FileExists(path.c_str())) load(path.c_str());
    }
    delete list;
}
}  // namespace

// @0x18c080 Game::LoadLanguageTable
bool Game::LoadLanguageTable(const char* file, const char* letter, double progress) {
    // (Render::CreateText(loadFont, letter) and Render::UpdateProgress(progress): the loading
    // screen, milestone 7)
    (void)letter;
    (void)progress;
    if (!StringTable::Init((std::string(kDataFolder) + file).c_str(), false, false)) return false;
    LoadFromActivePacks(file, [](const char* path) { StringTable::Init(path, false, false); });
    return true;
}

// @0x18cc4c Game::LoadSystemConfiguration
bool Game::LoadSystemConfiguration(bool (*loader)(const char*), const char* file, const char* letter,
                                   const char* name, double progress, unsigned failStep, unsigned okStep) {
    (void)letter;
    (void)name;
    (void)progress;
    (void)failStep;   // (LoadSampling::LoadStep, when not 0)
    (void)okStep;
    if (!loader((std::string(kDataFolder) + file).c_str())) return false;
    LoadFromActivePacks(file, loader);
    return true;
}

// @0x18564c Game::LoadEntityFactoryData
bool Game::LoadEntityFactoryData(const char* file) { return EntityFactory::LoadData(file); }

void ExitWithSendSave() { MainExit(); }

// (The listMaps debug string of the visited maps missing from the save, the expansion pack
// checks, the chapters, FileManager::SetGameState, the online player data and
// HUDWindow::UpdateTasks are not ported; main shows the HUD.)
bool LoadSavedGame(bool listMaps) {
    (void)listMaps;
    SaveManager::SaveBlock* state = SaveManager::GetGameStateData();
    if (!state) {
        // A new game: the hero (entity 0xc until the player chooses) with his squad and his
        // companion Stoutbeard (0x1a2), on campaign map 0x15, where the tutorial starts.
        GameState::Reset();
        Entity* hero = EntityManager::CreateEntity(0xc, false, true);
        hero->CreateSquad();
        hero->temporary = false;
        Entity* soldier = EntityManager::CreateEntity(0x1a2, false, true);
        SoldierSlots::AddSoldier(soldier);
        hero->GetSquad()->AddSoldier(soldier);
        soldier->temporary = false;
        SaveManager::SaveBlock* map = SaveManager::GetMapData(0x15);
        GameState::SetCurrentMapID(0x15);
        GameState::SetCurrentLocation(2);
        Map::Load(map, (uint32_t)Timer::GetGlobalTime(), false, false);
        return false;
    }
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
        EntityManager::OnTagUpdate(t);   // (its entities appear or are set up again)
        t->f05 = t->IsActive();
    }
    if (!changed) return;
    HUDWindow::UpdateTasks();
}
