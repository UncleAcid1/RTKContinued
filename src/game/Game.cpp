#include "engine/SoundsManager.h"
#include "hud/HUD.h"
#include "game/Game.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "engine/FileManager.h"
#include "engine/IconManager.h"
#include "engine/Render.h"
#include "engine/SystemFuncs.h"
#include "engine/Text.h"
#include "engine/Timer.h"
#include "game/BuildingHovers.h"
#include "game/BuildingMovement.h"
#include "game/BuildingPlacement.h"
#include "game/Entity.h"
#include "game/EntityData.h"
#include "game/GameState.h"
#include "game/LoadSampling.h"
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
    CastleTopWindow::ActivateFeatureHolders();
    // The first daily bonus ever (none received yet), once task 0x35c is done, with no window,
    // fight, hover, world dialog or pause: forced. As the original it never happens offline:
    // temporary setting 2 keeps its default 1 (only the online code sets it).
    if (GameState::GetReceivedDailyBonusCount() != 0 || !GameState::TaskCompleted(0x35c) ||
        SystemFuncs::GetTemporarySetting(2, 1) != 0)
        return;
    if (DailyBonusWindow::IsVisible() || WindowManager::GetShownWindowCount() != 0 || GameState::IsCombatActive() ||
        BuildingHovers::IsHoverVisible() || BuildingHovers::IsWorldDialogVisible() || GameState::IsPaused())
        return;
    DailyBonusWindow::ForceBonus();
    DailyBonusWindow::Show();
}

// @0x187674 turnMusic
void turnMusic(bool on) {
    if (!on) {
        SoundsManager::PauseAudio(true);
        return;
    }
    SoundsManager::PauseAudio(false);
    SoundsManager::PlayMusic(gMusicName.c_str(), true);
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

namespace {
Render::Font* g_loadFont = nullptr;     // 0x612388
Render::Sprite* g_loadText = nullptr;   // 0x612384
Render::GlowFilter g_loadGlow;          // 0x61238c
}  // namespace

void Game::InitLoadProgress() {
    g_loadFont = Render::CreateFont("fonts/ARICYRB.ttf", 0x14);
    g_loadText = nullptr;
    g_loadGlow = Render::GlowFilter();   // GlowFilter(1, 1, 1, 2, 5, 0, 0) @0x1fae08
    g_loadGlow.r = g_loadGlow.g = g_loadGlow.b = 1.f;
    g_loadGlow.blur = 2;
    g_loadGlow.strength = 5.f;
}

double Game::loadScreenSeconds = 0.0;

void Game::ShowLoadProgress(const char* letter, double progress) {
    std::u32string t(letter, letter + std::strlen(letter));
    g_loadText = Render::CreateText(g_loadFont, t.c_str(), 0.f, 0.f, 0.f, 1, true, &g_loadGlow, g_loadText,
                                    Render::kLayerGUI);
    // PORT: the bar glides from where it was to `progress` at loadScreenSeconds per full bar.
    static double shown = 0.0;
    if (loadScreenSeconds > 0.0 && progress > shown) {
        double last = Timer::GetTime();
        while (shown < progress) {
            SDL_Delay(16);
            const double now = Timer::GetTime();
            shown = std::min(progress, shown + (now - last) / loadScreenSeconds);
            last = now;
            SDL_PumpEvents();   // keeps the window responsive
            Render::UpdateProgress(shown, g_loadText, false, 0.2f);
        }
    }
    shown = std::max(shown, progress);
    Render::UpdateProgress(progress, g_loadText, false, 0.2f);
}

void Game::HideLoadProgress() {
    g_loadText = Render::RemoveSprite(g_loadText);
    Render::HideProgress();
}

// @0x18c080 Game::LoadLanguageTable
bool Game::LoadLanguageTable(const char* file, const char* letter, double progress) {
    ShowLoadProgress(letter, progress);
    if (!StringTable::Init((std::string(kDataFolder) + file).c_str(), false, false)) return false;
    LoadFromActivePacks(file, [](const char* path) { StringTable::Init(path, false, false); });
    return true;
}

// @0x18cc4c Game::LoadSystemConfiguration. PORT: the steps reach only Java's load sampling
// (FileManager::LoadStep), so the failure / success order of the two is not followed closely.
bool Game::LoadSystemConfiguration(bool (*loader)(const char*), const char* file, const char* letter,
                                   const char* name, double progress, unsigned failStep, unsigned okStep) {
    (void)name;
    ShowLoadProgress(letter, progress);
    if (!loader((std::string(kDataFolder) + file).c_str())) {
        if (failStep) LoadSampling::LoadStep(failStep);
        return false;
    }
    LoadFromActivePacks(file, loader);
    if (okStep) LoadSampling::LoadStep(okStep);
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
